#include <iostream>
#include "state_machine.hpp"

int main()
{

	std::cout << "Basic Test" << std::endl;
	{
		struct Input {
			int value;
		};
		struct Output {
			int value;
		};
		
		StateMachineManager<Input, Output> manager(Input{ 3 }, Output{ 2 }, 500);
		
		class AutomatonOne : public TimedObject<Input, Output> {
		public:
			virtual void tick(const Input &in, Output &out)
			{
				out.value = value_++;
			}
			int value_ = 0;
		};
		
		manager.addTimedObject(500, std::make_shared<AutomatonOne>());
		manager.unpause();
		
		for (int i = 0; i < 5; i++) {
			std::this_thread::sleep_for (std::chrono::seconds(1));
			auto out = manager.output();
			std::cout << "Value " << out->value << std::endl;
		}
	}
	
	std::cout << "Another test" << std::endl;
	{
#define TEST_2_AUTOMATON_COUNT 20
		struct Input {
			int value;
		};
		struct Output {
			int value[TEST_2_AUTOMATON_COUNT];
		};
		
		StateMachineManager<Input, Output> manager(Input{ 3 }, Output{ 2 }, 500);
		
		class AutomatonOne : public TimedObject<Input, Output> {
			Timer timer;
		public:
			virtual void tick(const Input &in, Output &out)
			{
				switch(status_) {
					case 0:
						if ((left && left->status_ > 1) || (right && right->status_ > 1)) {
							timer = makeTimer();
							status_ = 1;
						}
						break;
					case 1:
						if (timer.getTime() >= 1000)
							status_ = 2;
						break;
					case 2:
						if ((left && left->status_ > 1) && (right && right->status_ > 1)) {
							timer = makeTimer();
							status_ = 3;
						}
						break;
					case 3:
						if (timer.getTime() >= 1000)
							status_ = 4;
						break;
				}
				out.value[id_] = status_;
			}
			int status_ = 0;
			int id_;
			AutomatonOne *left = nullptr;
			AutomatonOne *right = nullptr;
			AutomatonOne()
			{
			
			}
		};
		
		std::vector<std::shared_ptr<AutomatonOne>> automatons;
		for (int i = 0; i < TEST_2_AUTOMATON_COUNT; i++) {
			automatons.push_back(std::make_shared<AutomatonOne>());
			if (i) {
				automatons[i - 1]->left = automatons[i].get();
				automatons[i]->right = automatons[i - 1].get();
			}
			automatons[i].get()->id_ = i;
		}
		automatons[0]->status_ = 2;
		
		
		for (int i = 0; i < TEST_2_AUTOMATON_COUNT; i++) {
			manager.addTimedObject(500, automatons[i]);
		}
		manager.unpause();
		
		for (int i = 0; i < 10; i++) {
			std::this_thread::sleep_for (std::chrono::seconds(1));
			auto out = manager.output();
			std::cout << "Values:";
			for (int i = 0; i < TEST_2_AUTOMATON_COUNT; i++) {
				std::cout << " " << out->value[i];
			}
			std::cout << std::endl;
		}
	}
	return 0;
}
