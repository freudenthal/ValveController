#pragma once

#include <ValveControl.h>
#include <Arduino.h>

#define IrrigationCoordinatorMaxValves 16

class IrrigationCoordinator
{
  public:
    enum class IrrigationCoordinatorMode : uint8_t
    {
      Idle,
      WaitingForBeginFinish,
      WaitingToStart,
      RunningCycle,
      Finishing,
      Count,
    };
    struct ValveState
    {
      bool HasActivatedThisCycle;
      bool HasCompletedThisCycle;
      uint32_t StartTime;
      uint32_t CycleTime;
    };
    typedef uint32_t ( *GetEpochTimeInSecondsFunction )();
    IrrigationCoordinator(ValveControl** InputValveControllers, uint8_t InputValveCount, ValveControl** OutputValveControllers, uint8_t OutputValveCount, ValveControl* TankValveController, ValveControl* TankBypassValveController);
    void SetGetTimeFunction(GetEpochTimeInSecondsFunction GetEpochTimeInSecondsFunctionToUse);
    void SetCycleRate(uint32_t TimeInSecondsBetweenCyclesToSet);
    void SetCycleActiveTime(uint32_t TimeInSecondsForCycleToSet);
    void SetCycleActiveTime(uint8_t Index, uint32_t TimeInSecondsForCycleToSet);
    void ForceCycleStart();
    void Begin();
    void Check();
    bool AllValvesAreIdle();
  private:
    ValveControl** InputValveControllers;
    uint8_t InputValveCount;
    ValveControl** OutputValveControllers;
    uint8_t OutputValveCount;
    ValveControl* TankValveController;
    ValveControl* TankBypassValveController;
    ValveState ValveProperties[IrrigationCoordinatorMaxValves];
    IrrigationCoordinatorMode Mode;
    GetEpochTimeInSecondsFunction GetEpochTimeInSeconds;
    uint32_t LastActivationTime;
    uint32_t TimeBetweenCycles;
    uint32_t TimeForCycle;
    bool ForceStart;
    void OpenInputValves();
    void CloseInputValves();
    void ResetValveProperties();
    bool WaitTimeIsComplete();
    bool CheckValveRunCycle();
    void StartCloseAllValves();
    void TransitionToIdle();
    void TransitionToWaitingForBeginFinish();
    void TransitionToWaitingToStart();
    void TransitionToRunningCycle();
    void TransitionToFinishing();
};