#include <iostream>
#include "state_machine.hpp"

int main()
{

	// Declare input and output structures
	struct Input {
		float temperature;
	};
	struct Output {
		float power;
	};
	
	// Declare a PID controller class
	class TemperatureController : public TimedObject<Input, Output> {
		const float proportional_ = 0.3f;
		const float integral_ = 0.02f;
		const float differential_ = -0.2f;
		float integralTotal = 0;
		float previous_ = 0;
	public:
		virtual void tick(const Input &in, Output &out)
		{
			float difference = desired_ - in.temperature;
			float needed = difference * proportional_ + integral_ * integralTotal + differential_ * (difference - previous_);
			
			if (needed < 0.0f)
				out.power = 0.0f;
			else if (needed > 100.0f)
				out.power = 100.0f;
			else {
				out.power = needed;
				integralTotal += difference;
			}
			previous_ = difference;
		}
		float desired_ = 0;
	};
	
	// Declare states for a class that would control the heating and cooling process
	enum TemperatureProgrammerState {
		STARTING = 0,
		HEATING,
		HOT,
		COOLING,
		COOL
	};

	// Declare the class for controlling the process, using the states set above
	class TemperatureProgrammer : public StateMachine<Input, Output, TemperatureProgrammerState> {
		float ramp_ = 0.005f;
		float max_ = 100.0f;
		int hotTime_ = 10000;
		float finish_ = 20.0f;
		float coolRamp_ = 0.005f;
		std::shared_ptr<TemperatureController> controller_;
		
	public:
		TemperatureProgrammer(std::shared_ptr<TemperatureController> controller) : controller_(controller)
		{
			state(STARTING);
		}
		
		virtual void tick(const Input &in, Output &out)
		{
			switch(state()) {
				case STARTING:
					state(HEATING);
					break;
				case HEATING: {
					float wanted = timeInState() * ramp_;
					if (wanted > max_) {
						wanted = max_;
						state(HOT);
					}
					controller_->desired_ = wanted;
					break;
				}
				case HOT:
					controller_->desired_ = max_;
					if (timeInState() > hotTime_) {
						state(COOLING);
					}
					break;
				case COOLING: {
					float wanted = max_ - timeInState() * ramp_;
					if (wanted < finish_) {
						wanted = finish_;
						state(COOL);
					}
					controller_->desired_ = wanted;
					break;
				}
				case COOL:
					break;
			}
		}
	};
	
	// Create the manager that controls all the state machines, initialise the Input/Output structures, set the minimal period to 100 ms
	// It starts in paused state
	StateMachineManager<Input, Output> manager(Input{ 20 }, Output{ 0 }, 100);
	
	// Create instances of the classes and insert them into the manager
	// It can be done only while the manager is paused
	std::shared_ptr<TemperatureController> controller = std::make_shared<TemperatureController>();
	manager.addTimedObject(200, controller);
	manager.addTimedObject(500, std::make_shared<TemperatureProgrammer>(controller));

	// Start the manager, this has to be done from the thread that created it
	manager.unpause();
	
	// Periodic reading of output and setting of input for the next turn
	for (int i = 0; i < 400; i++) {
		std::this_thread::sleep_for (std::chrono::milliseconds(100));
		auto out = manager.output(); // These two operations are thread-safe because of locks
		auto in = manager.input();
		in->temperature = 20 + (in->temperature - 20) * 0.95f + out->power;
		std::cout << "Power: " << out->power << " temperature " << in->temperature << " desired " << controller->desired_ << std::endl;
		// Destroying the returned structures unlocks the structures
	}
	
	// When the manager falls out of scope, it is safely destroyed
	return 0;
}


