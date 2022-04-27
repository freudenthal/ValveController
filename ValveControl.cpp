#include "ValveControl.h"

const float ValveControl::AllowablePositionError = 0.001;

ValveControl::ValveControl()
{
  Reset();
}
void ValveControl::Reset()
{
  Mode = ValveMode::Idle;
  BeginComplete = false;
  LastActivationTime = 0;
  ActivationInterval = 60*60*24;
  ActivationTimeSpan = 10*60;
  TimeRequiredToTransition = 30*1000*1000;
  PercentageOpenTarget = 0.0;
  IsValveClosed = false;
  IsValveOpen = false;
  ValveStateKnown = false;
  TransitionTimeStart = 0;
  LastTransitionTime = 0;
  PercentageOpen = NAN;
  PercentageOpenTarget = NAN;
  AutomaticModeActive = false;
  AutoModePercentageOpen = 1.0;
  AutomaticModeInAction = false;
}
void ValveControl::Begin(bool AllowMoveOnStart)
{
  Reset();
  if (SetEnableOutput == NULL)
  {
    Serial.println("<VALVEERROR>(SetEnableOutput can not be NULL)");
  }
  if (SetDirectionOutput == NULL)
  {
    Serial.println("<VALVEERROR>(SetDirectionOutput can not be NULL)");
  }
  if (GetOpenStatus == NULL)
  {
    Serial.println("<VALVEERROR>(GetOpenStatus can not be NULL)");
  }
  if (GetClosedStatus == NULL)
  {
    Serial.println("<VALVEERROR>(GetClosedStatus can not be NULL)");
  }
  if (GetEpochTimeInSeconds == NULL)
  {
    Serial.println("<VALVEERROR>(GetEpochTimeInSeconds can not be NULL)");
  }
  if (AllowMoveOnStart)
  {
    PercentageOpenTarget = 0.0;
    SetValveModeMoving();
  }
  else
  {
    Mode = ValveMode::Idle;
  }
  BeginComplete = true;
}
void ValveControl::Check()
{
  if (!BeginComplete)
  {
    Begin(false);
  }
  switch (Mode)
  {
    case ValveMode::Idle:
      break;
    case ValveMode::Waiting:
      CheckAutomationTime();
      break;
    case ValveMode::Moving:
      CheckOnPosition();
      break;
    default:
      break;
  }
}
void ValveControl::CheckAutomationTime()
{
  uint32_t CurrentTime = 0;
  if (GetEpochTimeInSeconds == NULL)
  {
    CurrentTime = GetEpochTimeInSeconds();
  }
  else
  {
    Serial.println("<VALVEERROR>(GetEpochTimeInSeconds can not be null.)");
    Mode = ValveMode::Idle;
    return;
  }
  if (AutomaticModeInAction)
  {
    if (CurrentTime - LastActivationTime > ActivationTimeSpan)
    {
      SetTargetPositionClosed();
      LastActivationTime = CurrentTime;
      FireActionCallback(ValveAction::ClosingAutomatically);
    }
  }
  else
  {
    if (CurrentTime - LastActivationTime > ActivationInterval)
    {
      SetTargetPosition(AutoModePercentageOpen);
      LastActivationTime = CurrentTime;
      FireActionCallback(ValveAction::OpeningAutomatically);
    }
  }
}
void ValveControl::CheckOnPosition()
{
  CheckPositionPins();
  CheckPositionToTargetPosition();
}
void ValveControl::FireActionCallback(ValveControl::ValveAction ActionType)
{
  if (ActionCallback != NULL)
  {
    ActionCallback(ActionType);
  }
}
void ValveControl::SetActionCallbackFunction(ActionCallbackFunction ActionCallbackToUse)
{
  ActionCallback = ActionCallbackToUse;
}
void ValveControl::SetActivationInterval(uint32_t Seconds)
{
  ActivationInterval = Seconds;
}
void ValveControl::SetActivationTimeSpan(uint32_t Seconds)
{
  ActivationTimeSpan = Seconds;
}
uint32_t ValveControl::GetActivationInverval()
{
  return ActivationInterval;
}
uint32_t ValveControl::GetActivationTimeSpan()
{
  return ActivationTimeSpan;
}
void ValveControl::SetTimeRequiredToTransition(uint32_t TimeInMicrosecondsToTransition)
{
 TimeRequiredToTransition = TimeInMicrosecondsToTransition;
}
uint32_t ValveControl::GetTimeRequiredToTransition()
{
  return TimeRequiredToTransition;
}
uint32_t ValveControl::GetLastTransitionTime()
{
  return LastTransitionTime;
}
void ValveControl::void SetVerbose(bool VerboseToSet)
{
  Verbose = VerboseToSet;
}
void ValveControl::SetValveModeMoving()
{
  Mode = ValveMode::Moving;
  TransitionTimeStart = micros();
}
void ValveControl::SetTargetPosition(float FractionToOpen)
{
  if (FractionToOpen < 0.0)
  {
    FractionToOpen = 0.0;
  }
  if (FractionToOpen > 1.0)
  {
    FractionToOpen = 1.0;
  }
  if (PercentageOpenTarget != FractionToOpen)
  {
    if (!ValveStateKnown)
    {
      if (FractionToOpen == 0.0)
      {
        PercentageOpenTarget = 1.0;
      }
      else if (FractionToOpen == 1.0)
      {
        PercentageOpenTarget = 0.0;
      }
      else
      {
        FractionToOpen = 0.0;
        PercentageOpenTarget = 1.0;
        if (Verbose)
        {
          Serial.println("Valve position unknown. Closing.");
        }
      }
    }
    float ChangeInTarget = PercentageOpenTarget - FractionToOpen;
    ChangeIsFullSpan = ChangeInTarget > 0.99;
    float AbsoluteChange = abs(ChangeInTarget);
    TransitionTimeToTarget = (uint32_t)(1000000*(AbsoluteChange*TimeRequiredToTransition));
    if (TransitionTimeToTarget > 0)
    {
      PercentageOpenTarget = FractionToOpen;
      SetValveModeMoving();
      float ChangeToPositionTarget = PercentageOpen - PercentageOpenTarget;
      bool NeedToOpenMore = ChangeToPositionTarget > 0.0;
      if (NeedToOpenMore)
      {
        SetDirectionOutput(true);
        ValveIsOpening = true;
      }
      else
      {
        SetDirectionOutput(false);
        ValveIsOpening = false;
      }
      SetEnableOutput(true);
      ValveEnabled = true;
      if (Verbose)
      {
        Serial.print("Valve moving. Change is ");
        Serial.print(ChangeInTarget,2);
        Serial.print(". Time to change is ");
        Serial.print(TransitionTimeToTarget);
        Serial.print(". Direction is ");
        Serial.print(NeedToOpenMore);
        Serial.println(".");
      }
    }
    CheckPositionPins();
    CheckPositionToTargetPosition();
  }
}
void ValveControl::CheckPositionPins()
{
  bool ValveOpenConfirmed = GetOpenStatus();
  bool ValveClosedConfirmed = GetClosedStatus();
  if (ValveClosedConfirmed || ValveOpenConfirmed)
  {
    ValveStateKnown = true;
  }
  IsValveOpen = ValveOpenConfirmed;
  IsValveClosed = ValveClosedConfirmed;
  if (Verbose)
  {
    Serial.print("Valve opened is ");
    Serial.print(IsValveOpen);
    Serial.print(" and valve closed is ");
    Serial.print(IsValveClosed);
    Serial.print(".");
  }
  if (ValveClosedConfirmed && (PercentageOpen != 0.0))
  {
    PercentageOpen = 0.0;
    FireActionCallback(ValveAction::Closed);
  }
  if (ValveOpenConfirmed && (PercentageOpen != 1.0))
  {
    PercentageOpen = 1.0;
    FireActionCallback(ValveAction::Opened);
  }
}
void ValveControl::CheckPositionToTargetPosition()
{
  if (PercentageOpenTarget == PercentageOpen)
  {
    StopMoving();
    FireActionCallback(ValveAction::AtPositionTarget);
  }
  else if ((PercentageOpenTarget == 1.0) || (PercentageOpenTarget == 0.0))
  {

  }
  else if (ValveEnabled)
  {
    uint32_t TimeSinceMoveStart = micros() - LastActivationTime;
    float EstimatedAbsolutePercentageMoved = float(TimeSinceMoveStart) / TimeRequiredToTransition;
    float EstimatedPercentageMoved = ValveIsOpening ? EstimatedAbsolutePercentageMoved : -1.0*EstimatedAbsolutePercentageMoved;
    float EstimatedPercentageOpen = PercentageOpen + EstimatedPercentageMoved;
    if (EstimatedPercentageOpen < 0.0)
    {
      EstimatedPercentageOpen = 0.0;
    }
    if (EstimatedPercentageOpen > 1.0)
    {
      EstimatedPercentageOpen = 1.0;
    }
    bool StopMovementConditionMet = false;
    if ( (EstimatedPercentageOpen >= PercentageOpenTarget) && ValveIsOpening)
    {
      StopMovementConditionMet = true;
    }
    if ( (EstimatedPercentageOpen <= PercentageOpenTarget) && (!ValveIsOpening) )
    {
      StopMovementConditionMet = true;
    }
    if (StopMovementConditionMet)
    {
      PercentageOpen = EstimatedPercentageOpen;
      StopMoving();
      FireActionCallback(ValveAction::AtPositionTarget);
    }
  }
}

void ValveControl::StopMoving()
{
  SetEnableOutput(false);
  LastTransitionTime = micros() - TransitionTimeStart;
  if (Verbose)
  {
    Serial.print("Valve transition time ");
    Serial.print(LastTransitionTime);
    Serial.println("us.");
  }
  ValveEnabled = false;
  if (AutomaticModeActive)
  {
    Mode = ValveMode::Waiting;
  }
  else
  {
    Mode = ValveMode::Idle;
  }
}

float ValveControl::GetTargetPosition()
{
  return PercentageOpenTarget;
}
float ValveControl::GetPosition()
{
  return PercentageOpen;
}
void ValveControl::SetTargetPositionOpen()
{
  SetTargetPosition(1.0);
}
void ValveControl::SetTargetPositionClosed()
{
  SetTargetPosition(0.0);
}
void ValveControl::SetAutomaticModeActive(bool AutomaticModeActiveToSet)
{
  AutomaticModeActive = AutomaticModeActiveToSet;
}
bool ValveControl::GetAutomaticModeActive()
{
  return AutomaticModeActive;
}
void ValveControl::SetAutomaticModePercentageOpen(float AutomaticModePercentageToSet)
{
  AutoModePercentageOpen = AutomaticModePercentageToSet;
}
float ValveControl::GetAutomaticModePercentageOpen()
{
  return AutoModePercentageOpen;
}
void ValveControl::SetPinFunctions(SetPinOutputFunction EnableOutputFunctionToUse, SetPinOutputFunction DirectionOutputFunctionToUse, GetPinInputFunction GetOpenStatusFunctionToUse, GetPinInputFunction GetCosedStatusFunctionToUse)
{
  SetEnableOutput = EnableOutputFunctionToUse;
  SetDirectionOutput = DirectionOutputFunctionToUse;
  GetOpenStatus = GetOpenStatusFunctionToUse;
  GetClosedStatus = GetCosedStatusFunctionToUse;
}
void ValveControl::SetGetTimeFunction(GetEpochTimeInSecondsFunction GetEpochTimeInSecondsFunctionToUse)
{
  GetEpochTimeInSeconds = GetEpochTimeInSecondsFunctionToUse;
}

