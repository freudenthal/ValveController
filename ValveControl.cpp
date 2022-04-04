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
  NextActivationTime = 0;
  ActivationInterval = 60*60*24;
  ActivationTimeSpan = 10*60;
  TimeRequiredToTransition = 30*1000*1000;
  PercentageOpenTarget = 0.0;
  IsValveClosed = false;
  IsValveOpen = false;
  ValveStateKnown = false;
  TransitionTimeStart = 0;
  PercentageOpen = float.NaN;
  PercentageOpenTarget = float.NaN;
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
    PositionTarget = 0.0;
    Mode = ValveMode::Moving;
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
  uint32_t CurrentTime = GetEpochTimeInSeconds;
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
    FireAction(ActionType);
  }
}
void ValveControl::void SetActionCallbackFunction(ActionCallbackFunction ActionCallbackToUse)
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
        PercentageOpen = 1.0;
      }
      else if (FractionToOpen == 1.0)
      {
        PositionOpen = 0.0;
      }
      else
      {
        FractionToOpen = 0.0;
        PositionOpen = 1.0;
        Serial.println("Valve position unknown. Closing.");
      }
    }
    float ChangeInTarget = PercentageOpenTarget - FractionToOpen;
    float AbsoluteChange = abs(ChangeInTarget);
    TransitionTimeToTarget = (uint32_t)(1000000*(AbsoluteChange*TimeRequiredToTransition));
    if (TransitionTimeToTarget > 0)
    {
      PercentageOpenTarget = FractionToOpen;
      Mode = ValveMode::Moving;
      TransitionTimeStart = micros();
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
  if (ValveClosedConfirmed && (PercentageOpen != 0.0))
  {
    PercentageOpen = 0.0;
    IsValveClosed = true;
    if (ActionCallback != NULL)
    {
      ActionCallback(ValveAction::Closed)
    }
  }
  if (ValveOpenConfirmed && (PercentageOpen != 1.0))
  {
    PercentageOpen = 1.0;
    if (ActionCallback != NULL)
    {
      ActionCallback(ValveAction::Opened)
    }
  }
}
void ValveControl::CheckPositionToTargetPosition()
{
  if (PercentageOpenTarget == PercentageOpen)
  {
    StopMoving();
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
    if (EstimatedPercentageOpen >= PositionTarget && ValveIsOpening)
    {
      StopMovementConditionMet = true;
    }
    if (EstimatedPercentageOpen <= PositionTarget && (!ValveIsOpening) )
    {
      StopMovementConditionMet = true;
    }
    if (StopMovementConditionMet)
    {
      PercentageOpen = EstimatedPercentageOpen;
      StopMovement();
      if (ActionCallback != NULL)
      {
        ActionCallback(ValveAction::AtPositionTarget)
      }
    }
  }
}

void ValveControl::StopMoving()
{
  SetEnableOutput(false);
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
bool ValveControl::GetAutomaticModeActive(bool AutomaticModeActiveToSet)
{
  return AutomaticModeActive;
}
void ValveControl::SetAutomaticModePercentageOpen(float AutomaticModePercentageToSet)
{
  return AutoModePercentageOpen;
}
void ValveControl::SetPinFunctions(SetPinOutputFunction EnableOutputFunctionToUse, SetPinOutputFunction DirectionOutputFunctionToUse, GetPinInputFunction GetOpenStatusFunctionToUse, GetPinInputFunction GetCosedStatusFunctionToUse,)
{
  SetEnableOutput = EnableOutputFunctionToUse;
  SetDirectionOutput = DirectionOutputFunctionToUse;
  GetOpenStatus = GetOpenStatusFunctionToUse;
  GetClosedStatus = GetCosedStatusFunctionToUse;
}
void ValveControl::SetGetTimeFunction(GetTimeInSecondsFunction GetEpochTimeInSecondsFunctionToUse)
{
  GetEpochTimeInSeconds = GetEpochTimeInSecondsFunctionToUse;
}
