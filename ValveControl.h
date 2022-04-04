#ifndef VALVECONTROL_H
#define VALVECONTROL_H

#include <Arduino.h>

class ValveControl
{
  public:
    enum class ValveMode : uint8_t
    {
      Idle,
      Waiting,
      Moving,
      Count,
    };
    enum class ValveAction : uint8_t
    {
      OpeningAutomatically = 0,
      ClosingAutomatically = 1,
      Opened = 2,
      Closed = 3,
      AtPositionTarget = 4,
      Count = 5,
    };
    typedef void ( *ActionCallbackFunction )(ValveAction Action);
    typedef void ( *SetPinOutputFunction )(bool OutputToUse);
    typedef bool ( *GetPinInputFunction )();
    typedef uint32_t ( *GetEpochTimeInSecondsFunction )();

    ValveControl();
    void Begin(bool AllowMoveOnStart);
    void Check();
    void SetActivationInterval(uint32_t Seconds);
    void SetActivationTimeSpan(uint32_t Seconds);
    uint32_t GetActivationInverval();
    uint32_t GetActivationTimeSpan();
    void SetTimeRequiredToTransition(uint32_t TimeInMicrosecondsToTransition);
    uint32_t GetTimeRequiredToTransition();
    void SetTargetPosition(float FractionToOpen);
    float GetTargetPosition();
    float GetPosition();
    void SetTargetPositionOpen();
    void SetTargetPositionClosed();
    void SetAutomaticModeActive(bool AutomaticModeActiveToSet);
    bool GetAutomaticModeActive();
    void SetAutomaticModePercentageOpen(float AutomaticModePercentageToSet);
    bool GetAutomaticModePercentageOpen();
    void SetPinFunctions
    (
      SetPinOutputFunction EnableOutputFunctionToUse,
      SetPinOutputFunction DirectionOutputFunctionToUse,
      GetPinInputFunction GetOpenStatusFunctionToUse,
      GetPinInputFunction GetCosedStatusFunctionToUse,
    );
    void SetGetTimeFunction(GetTimeInSecondsFunction GetEpochTimeInSecondsFunctionToUse);
    void SetActionCallbackFunction(ActionCallbackFunction ActionCallbackToUse);
  private:
    void Reset();
    static const float AllowablePositionError;
    SetPinOutputFunction SetEnableOutput;
    SetPinOutputFunction SetDirectionOutput;
    GetPinInputFunction GetOpenStatus;
    GetPinInputFunction GetClosedStatus;
    GetTimeInSecondsFunction GetEpochTimeInSeconds;
    ActionCallbackFunction ActionCallback;
    ValveMode Mode;
    bool BeginComplete;
    uint32_t LastActivationTime;
    uint32_t ActivationInterval;
    uint32_t ActivationTimeSpan;
    uint32_t TimeRequiredToTransition;
    uint32_t TransitionTimeStart;
    uint32_t TransitionTimeToTarget;
    bool IsValveClosed;
    bool IsValveOpen;
    bool ValveStateKnown;
    bool ValveEnabled;
    bool ValveIsOpening;
    float PercentageOpen;
    float PercentageOpenTarget;
    bool AutomaticModeActive;
    float AutoModePercentageOpen;
    bool AutomaticModeInAction;
};

#endif