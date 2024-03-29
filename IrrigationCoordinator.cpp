#include "IrrigationCoordinator.h"

const uint8_t IrrigationCoordinator::Events0DefaultValesToActivateCount = 9;
const uint8_t IrrigationCoordinator::Events0DefaultValesToActivate[9] = {15,14,13,12,11,10,9,8,7};

IrrigationCoordinator::IrrigationCoordinator(ValveControl** InputValveControllers, uint8_t InputValveCount, ValveControl** OutputValveControllers, uint8_t OutputValveCount, ValveControl* TankValveController, ValveControl* TankBypassValveController)
{
	this->InputValveControllers = InputValveControllers;
	this->InputValveCount = InputValveCount;
	this->OutputValveControllers = OutputValveControllers;
	this->OutputValveCount = OutputValveCount;
	this->TankValveController = TankValveController;
	this->TankBypassValveController = TankBypassValveController;
	for (uint8_t Index=0;Index<InputValveCount;Index++)
	{
		InputValveControllers[Index]->SetAutomaticModeActive(false);
	}
	for (uint8_t Index=0;Index<OutputValveCount;Index++)
	{
		OutputValveControllers[Index]->SetAutomaticModeActive(false);
	}
	for (uint8_t Index=0;Index<CycleEventMaxCount;Index++)
	{
		Events[Index].CycleActive = false;
		Events[Index].UseTank = false;
		Events[Index].TankSplit = 0.0;
		Events[Index].LastActivationTime = 0;
		Events[Index].WaitingTime = 0;
		Events[Index].OnTime = 0;
		Events[Index].ValvesToActivate = NULL;
		Events[Index].ValvesToActivateCount = 0;
	}
	Events[0].CycleActive = true;
	Events[0].UseTank = false;
	Events[0].TankSplit = 0.0;
	Events[0].WaitingTime = 60*60*24;
	Events[0].OnTime = 30*60;
	Events[0].LastActivationTime = 0;
	Events[0].ValvesToActivate = const_cast<uint8_t*>(Events0DefaultValesToActivate);
	Events[0].ValvesToActivateCount = Events0DefaultValesToActivateCount;
	EventCount = 1;
	TankValveController->SetAutomaticModeActive(false);
	TankBypassValveController->SetAutomaticModeActive(false);
	ForceStart = false;
	TimeBetweenCycles = 60*60*24;
	TimeForCycle = 10*60;
	for (uint8_t Index=0;Index<OutputValveCount;Index++)
	{
		ValveProperties[Index].HasActivatedThisCycle = false;
		ValveProperties[Index].HasCompletedThisCycle = false;
		ValveProperties[Index].StartTime = 0;
		ValveProperties[Index].CycleTime = TimeForCycle;
	}
}
void IrrigationCoordinator::ForceCycleStart(uint8_t EventIndex)
{
	if ( (EventIndex < EventCount) && (Events[EventIndex].ValvesToActivate != NULL) && (Events[EventIndex].ValvesToActivateCount > 0) )
	{
		TimeBetweenCycles = Events[EventIndex].WaitingTime;
		TimeForCycle = Events[EventIndex].OnTime;
		ValvesToActivate = Events[EventIndex].ValvesToActivate;
		ValvesToActivateCount = Events[EventIndex].ValvesToActivateCount;
		ForceStart = true;
	}
}
void IrrigationCoordinator::SetGetTimeFunction(GetEpochTimeInSecondsFunction GetEpochTimeInSecondsFunctionToUse)
{
	GetEpochTimeInSeconds = GetEpochTimeInSecondsFunctionToUse;
}
void IrrigationCoordinator::SetCycleRate(uint32_t TimeInSecondsBetweenCyclesToSet)
{
	TimeBetweenCycles = TimeInSecondsBetweenCyclesToSet;
}
void IrrigationCoordinator::SetCycleActiveTime(uint32_t TimeInSecondsForCycleToSet)
{
	TimeForCycle = TimeInSecondsForCycleToSet;
	for (uint8_t Index=0;Index<OutputValveCount;Index++)
	{
		ValveProperties[Index].CycleTime = TimeForCycle;
	}
}
void IrrigationCoordinator::SetCycleActiveTime(uint8_t Index, uint32_t TimeInSecondsForCycleToSet)
{
	if (Index < OutputValveCount)
	{
		ValveProperties[Index].CycleTime = TimeInSecondsForCycleToSet;
	}
}
void IrrigationCoordinator::StartCloseAllValves()
{
	for (uint8_t Index=0;Index<InputValveCount;Index++)
	{
		InputValveControllers[Index]->SetTargetPositionClosed();
	}
	for (uint8_t Index=0;Index<OutputValveCount;Index++)
	{
		OutputValveControllers[Index]->SetTargetPositionClosed();
	}
	TankValveController->SetTargetPositionClosed();
	TankBypassValveController->SetTargetPositionClosed();
}
void IrrigationCoordinator::Begin()
{
	StartCloseAllValves();
	TransitionToWaitingForBeginFinish();
}
bool IrrigationCoordinator::AllValvesAreIdle()
{
	for (uint8_t Index=0;Index<InputValveCount;Index++)
	{
		if (InputValveControllers[Index]->GetMode() != ValveControl::ValveMode::Idle)
		{
			return false;
		}
	}
	for (uint8_t Index=0;Index<OutputValveCount;Index++)
	{
		if (OutputValveControllers[Index]->GetMode() != ValveControl::ValveMode::Idle)
		{
			return false;
		}
	}
	if (TankValveController->GetMode() != ValveControl::ValveMode::Idle)
	{
		return false;
	}
	if (TankBypassValveController->GetMode() != ValveControl::ValveMode::Idle)
	{
		return false;
	}
	return true;
}
bool IrrigationCoordinator::WaitTimeIsComplete()
{
	if (GetEpochTimeInSeconds != NULL)
	{
		for (uint8_t EventIndex = 0; EventIndex < EventCount; EventIndex++)
		{
			if (Events[EventIndex].LastActivationTime - GetEpochTimeInSeconds() > Events[EventIndex].WaitingTime)
			{
				if ( (!Events[EventIndex].CycleActive) && (Events[EventIndex].ValvesToActivate != NULL) && (Events[EventIndex].ValvesToActivateCount > 0))
				{
					Events[EventIndex].CycleActive = true;
					Events[EventIndex].LastActivationTime = GetEpochTimeInSeconds();
					Serial.print("<IRRIGCON>(Starting watering cycle ");
					Serial.print(EventIndex);
					Serial.println(".)");
					TimeBetweenCycles = Events[EventIndex].WaitingTime;
					TimeForCycle = Events[EventIndex].OnTime;
					ValvesToActivate = Events[EventIndex].ValvesToActivate;
					ValvesToActivateCount = Events[EventIndex].ValvesToActivateCount;
					TransitionToRunningCycle();
				}
			}
		}
		
		return true;
	}
	else
	{
		Serial.println("<IRRIGCONERROR>(GetEpochTimeInSeconds can not be null.)");
		TransitionToIdle();
		return false;
	}
}
bool IrrigationCoordinator::CheckValveRunCycle()
{
	uint32_t CurrentTime = 0;
	if (AllValvesAreIdle())
	{
		for (uint8_t Index=0;Index<OutputValveCount;Index++)
		{
			if (!ValveProperties[Index].HasCompletedThisCycle  && ValveProperties[Index].WillBeActivatedThisCycle)
			{
				if (!ValveProperties[Index].HasActivatedThisCycle)
				{
					if (GetEpochTimeInSeconds != NULL)
					{
						CurrentTime = GetEpochTimeInSeconds();
					}
					else
					{
						Serial.println("Irrigation Coordinator can not get time.");
						CurrentTime = millis()/1000;
					}
					ValveProperties[Index].HasActivatedThisCycle = true;
					OutputValveControllers[Index]->SetTargetPositionOpen();
					ValveProperties[Index].StartTime = CurrentTime;
					ValveProperties[Index].HasCompletedThisCycle = false;
					TimeForCycle = ValveProperties[Index].CycleTime;
					Serial.print("Opening valve index ");
					Serial.print(Index);
					Serial.println(".");
					return false;
				}
				else
				{
					if ( (CurrentTime - ValveProperties[Index].StartTime) > ValveProperties[Index].CycleTime)
					{
						ValveProperties[Index].HasCompletedThisCycle = true;
						OutputValveControllers[Index]->SetTargetPositionClosed();
						Serial.print("Closing valve index ");
						Serial.print(Index);
						Serial.println(".");
						return false;
					}
				}
			}
		}
	}
	return true;
}
void IrrigationCoordinator::OpenInputValves()
{
	InputValveControllers[0]->SetTargetPositionOpen();
	TankBypassValveController->SetTargetPositionOpen();
}
void IrrigationCoordinator::CloseInputValves()
{
	for (uint8_t Index=0;Index<InputValveCount;Index++)
	{
		InputValveControllers[Index]->SetTargetPositionClosed();
	}
	TankBypassValveController->SetTargetPositionClosed();
}
void IrrigationCoordinator::ResetValveProperties()
{
	for (uint8_t Index=0;Index<OutputValveCount;Index++)
	{
		ValveProperties[Index].HasActivatedThisCycle = false;
		ValveProperties[Index].HasCompletedThisCycle = false;
		ValveProperties[Index].WillBeActivatedThisCycle = false;
		ValveProperties[Index].StartTime = 0;
	}
	for (uint8_t Index=0;Index<ValvesToActivateCount;Index++)
	{
		uint8_t ValveIndex = ValvesToActivate[Index];
		ValveProperties[ValveIndex].WillBeActivatedThisCycle = true;
	}
}
void IrrigationCoordinator::Check()
{
	switch(Mode)
	{
		case IrrigationCoordinatorMode::Idle:
			break;
		case IrrigationCoordinatorMode::WaitingForBeginFinish:
			if (AllValvesAreIdle())
			{
				TransitionToWaitingToStart();
			}
			break;
		case IrrigationCoordinatorMode::WaitingToStart:
			if (WaitTimeIsComplete() || ForceStart)
			{
				ForceStart = false;
				TransitionToRunningCycle();
			}
			break;
		case IrrigationCoordinatorMode::RunningCycle:
			if (CheckValveRunCycle())
			{
				TransitionToFinishing();
			}
			break;
		case IrrigationCoordinatorMode::Finishing:
			if (AllValvesAreIdle())
			{
				TransitionToWaitingToStart();
			}
			break;
		default:
			break;
	}
}
void IrrigationCoordinator::TransitionToIdle()
{
	Serial.println("Irrigation constroller switching to idle.");
	Mode = IrrigationCoordinatorMode::Idle;
}
void IrrigationCoordinator::TransitionToWaitingForBeginFinish()
{
	Serial.println("Irrigation constroller switching waiting on begin complete.");
	Mode = IrrigationCoordinatorMode::WaitingForBeginFinish;
}
void IrrigationCoordinator::TransitionToWaitingToStart()
{
	if (GetEpochTimeInSeconds != NULL)
	{
		LastActivationTime = GetEpochTimeInSeconds();
	}
	else
	{
		Serial.println("<IRRIGCONERROR>(GetEpochTimeInSeconds can not be null.)");
		TransitionToIdle();
		return;
	}
	Mode = IrrigationCoordinatorMode::WaitingToStart;
}
void IrrigationCoordinator::TransitionToRunningCycle()
{
	Serial.println("Irrigation constroller switching to cycle start.");
	Mode = IrrigationCoordinatorMode::RunningCycle;
	ResetValveProperties();
	OpenInputValves();
	CheckValveRunCycle();
}
void IrrigationCoordinator::TransitionToFinishing()
{
	Serial.println("Irrigation constroller switching to cycle finish.");
	Mode = IrrigationCoordinatorMode::Finishing;
	StartCloseAllValves();
}