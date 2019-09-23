/*
* \brief Tools to use a thread like a finite-state automaton-like program that PLC and automation programmers like.
*
* The class has a basic weakeup period that is set as a constructor argument, in milliseconds. Then, it can be filled
* with objects whose timing method is activated periodically (the period is set when adding, the parent class' timer
* must be its divisor). A modification that implements typical functionality of a finite state automaton is available.
* When all is set up, the resume() method is called to activate it all. Each wakeup gets the same input and time for
* all automatons, that are fired in the sequence they were added in.
*
* Input and output are be accessed in a synchronised way, either between wakeups or delayed until the wakeup finishes.
* The returned value is a smart pointer that keeps the wakeup from occurring until destroyed. You may want to copy it.
*
* Most classes expect two template arguments, one for the input structure given to the state machines, one for the
* output structures given to the state machines. Class StateMachine that comfortably implements state machines accepts
* the type describing its state (meant to be an enum) as the third template argument.
*/

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "looping_thread/looping_thread.hpp"
#include <vector>

#include <iostream>

template<typename Input, typename Output>
class TimedObject {
protected:
	long long timeOfLastFreeze_;
	int timeIncrease_;
	virtual void setupTurn(long long time)
	{
		timeIncrease_ = int(time - timeOfLastFreeze_);
		timeOfLastFreeze_ = time;
	}
public:
	/*!
	* \brief Returns the time between the current step and the previous one
	*
	* \return The time in milliseconds
	*/
	int lastPeriod()
	{
		return timeIncrease_;
	}
	
	/*!
	* \brief Returns the current time, kept at once value during the whole tick
	*
	* \return The time in milliseconds
	*/
	long long frameTime()
	{
		return timeOfLastFreeze_;
	};
	
	class Timer {
		long long since_;
		TimedObject<Input, Output> *parent_;
		Timer(long long since, TimedObject<Input, Output> *parent) : since_(since), parent_(parent)
		{
		}
		template<typename In, typename Out> friend class TimedObject;
	public:
	
		/*!
		* \brief Default constructor, default constructed or disabled timer always returns 0 as time
		*/
		Timer() : parent_(nullptr)
		{
		}
		
		/*!
		* \brief Returns the time since this timer was created
		*
		* \return The time in milliseconds
		*/
		long long time()
		{
			if(!parent_) return 0;
			return parent_->timeOfLastFreeze_ - since_;
		}
		
		/*!
		* \brief Returns uf the time is active, that is, wasn't default-constructed or disabled
		*
		* \return If it is enabled
		*/
		bool active()
		{
			return (parent_ != nullptr);
		}
		
		/*!
		* \brief Disables the timer so that it will not be active and always return time 0
		*/
		void deactivate()
		{
			parent_ = nullptr;
		}
	};
	
	/*!
	* \brief Returns a timer measuring time from the moment it was returned
	*
	* \return The timer, use its getTime() method to get the time in milliseconds
	*/
	Timer makeTimer()
	{
		return Timer(timeOfLastFreeze_, this);
	}
	
	/*!
	* \brief Overload this function with a function you want to be called periodically
	*
	* \param The input structure, its type is the first template argument
	* \param The output structure, its type is the second template argument
	*/
	virtual void tick(const Input &in, Output &out) = 0;
	
	template<typename In, typename Out> friend class StateMachineManager;
};

template<typename Input, typename Output, typename State>
class StateMachine : public TimedObject<Input, Output> {
	long long stateTimer_ = 0;
	enum class StateChangedType : std::uint8_t  {
		THIS_TICK,
		PREVIOUS_TICK,
		BEFORE
	};
	StateChangedType stateChanged_ = StateChangedType::THIS_TICK;
	virtual void setupTurn(long long time)
	{
		TimedObject<Input, Output>::setupTurn(time);
		stateTimer_ += TimedObject<Input, Output>::timeIncrease_;
		if(stateChanged_ == StateChangedType::THIS_TICK)
			stateChanged_ = StateChangedType::PREVIOUS_TICK;
		else if(stateChanged_ == StateChangedType::PREVIOUS_TICK)
			stateChanged_ = StateChangedType::BEFORE;
	}
	State state_;
	
protected:
	/*!
	* \brief Returns the current state of the automaton, the state's type is set as the third template argument
	*
	* \return The state
	*
	* \note Must be called in the final object's constructor to set its initial state
	*/
	State state()
	{
		return state_;
	}
	
	/*!
	* \brief Changes the state of the automaton
	*
	* \param The new state
	*/
	void state(State newState)
	{
		if(state_ == newState) return;
		state_ = newState;
		stateChanged_ = StateChangedType::THIS_TICK;
		stateTimer_ = 0;
	}
	
	/*!
	* \brief Returns the time since the last change of state
	*
	* \return The time in milliseconds
	*/
	long long timeInState()
	{
		return stateTimer_;
	}
	
	/*!
	* \brief Returns true if the automaton is running its first tick in the current state
	*
	* \return If this is the first tick after state change
	*/
	bool afterStateChange()
	{
		return (stateChanged_ == StateChangedType::PREVIOUS_TICK);
	}
};

template<typename T>
class ProtectedReturn {
	std::function<void()> onRelease_;
	T *content_;
	ProtectedReturn(T *content, std::function<void()> onRelease) :
		content_(content), onRelease_(onRelease)
	{
	}
	template<typename T2>
	ProtectedReturn(const ProtectedReturn<T2> &) = delete;
	template<typename T2>
	ProtectedReturn<T> &operator=(const ProtectedReturn<T2> &) = delete;
public:
	/*!
	* \brief Destructor, stops deferring the ticks
	*/
	~ProtectedReturn()
	{
		onRelease_();
	}
	
	/*!
	* \brief Gives access to the structure
	*/
	T *operator->()
	{
		return content_;
	}
	
	/*!
	* \brief Gives access to the const structure
	*/
	const T *operator->() const
	{
		return content_;
	}
	
	/*!
	* \brief Gives access to the structure
	*
	* \note This is meant to be used to assign into it to replace the whole structure and destroy this object in the following line
	*/
	T &operator*()
	{
		return *content_;
	}
	
	/*!
	* \brief Gives access to the structure
	*
	* \note This is meant to be used to assign the whole structure into a copy and destroy this object in the following line
	*/
	const T &operator*() const
	{
		return *content_;
	}
	
	template<typename In, typename Out> friend class StateMachineManager;
};

template<typename Input, typename Output>
class StateMachineManager {
	std::vector<std::pair<int, std::shared_ptr<TimedObject<Input, Output>>>> machines_;
	Input input_;
	Output output_;
	int tickOrder_ = 0;
	int period_;
	int paused_;
	std::mutex inputMutex_;
	std::mutex outputMutex_;
	std::mutex pauseMutex_;
	std::function<void(Input &)> inputTrigger_;
	std::function<void(const Output &)> outputTrigger_;
	std::unique_ptr<LoopingThread> loop_;
	void tick()
	{
		Input input;
		Output output = output_; // It's const in the other thread
		if(inputTrigger_)
			inputTrigger_(input_);
		{
			std::unique_lock<std::mutex> lock(inputMutex_);
			input = input_;
		}
		long long start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		for(unsigned int i = 0; i < machines_.size(); i++)
			if(tickOrder_ % machines_[i].first == 0) {
				machines_[i].second->setupTurn(start);
				machines_[i].second->tick(input, output);
			}
		tickOrder_++;
		{
			std::unique_lock<std::mutex> lock(outputMutex_);
			output_ = output;
		}
		if(outputTrigger_)
			outputTrigger_(output_);
	}
public:

	/*!
	* \brief The constructor, leaves the thread in a paused state
	*
	* \param The initial input structure
	* \param The initial output structure
	* \param The base period that divides all other periods of inserted objects, in millisecond
	*
	* \note The execution starts paused, it will have to be unpaused after inserting the contents
	*/
	StateMachineManager(Input input, Output output, int basePeriod) :
	input_(input),
	output_(output),
	period_(basePeriod),
	paused_(1)
	{
	}
	
	/*!
	* \brief Adds a class derived from StateMachine or TimedObject if unusual behaviour is needed
	*
	* \param The period in milliseconds, must be divisible by the base period
	* \param A shared pointer to the object
	*
	* \note The execution must be paused to call this safely
	*/
	void addTimedObject(int period, std::shared_ptr<TimedObject<Input, Output>> added)
	{
		machines_.push_back(std::make_pair(period / period_, added));
	}
	
	/*!
	* \brief Removes a timed object from the system
	*
	* \param A shared pointer to the object
	*
	* \note The execution must be paused to call this safely
	*/
	void removeTimedObject(std::shared_ptr<TimedObject<Input, Output>> removed)
	{
		std::remove(machines_.begin(), machines_.end(), [&removed](const std::pair<int, std::shared_ptr<TimedObject<Input, Output>>> &tried) {
			return (removed == tried.second);
		});
	}
	
	/*!
	* \brief Pauses execution, must be resumed with unpause(), if paused twice, it will have to be unpaused twice, making pausing reentrant
	*/
	void pause()
	{
		std::lock_guard<std::mutex> lock(pauseMutex_);
		if(!paused_) {
			loop_.release();
			paused_ = 1;
		}
		else paused_++;
	}
	
	/*!
	* \brief Resumes execution paused by pause(), if paused twice, it will have to be unpaused twice, making pausing reentrant
	*
	* \note Must be called after the constructor when all is set up
	*/
	void unpause()
	{
		std::lock_guard<std::mutex> lock(pauseMutex_);
		if(paused_ == 1) {
			loop_ = std::make_unique<LoopingThread>(std::chrono::milliseconds(period_), [this]()
			{
				tick();
			});
			paused_ = 0;
		}
		else paused_--;
	}
	
	/*!
	* \brief Returns the input structure and holds it until the returned smart pointer is destroyed
	*
	* \return A smart pointer to the input structure, must be destroyed asap to avoid disturbing the execution
	*/
	ProtectedReturn<Input> input()
	{
		std::shared_ptr<std::unique_lock<std::mutex>> lock = std::make_unique<std::unique_lock<std::mutex>>(inputMutex_);
		return ProtectedReturn<Input>(&input_, [lock]() { /* Keep a copy of the mutex pointer */ });
	}
	
	/*!
	* \brief Returns the output structure and holds it in that state until the returned smart pointer is destroyed
	*
	* \return A smart pointer to the output structure, must be destroyed asap to avoid disturbing the execution
	*/
	const ProtectedReturn<Output> output()
	{
		std::shared_ptr<std::unique_lock<std::mutex>> lock = std::make_unique<std::unique_lock<std::mutex>>(outputMutex_);
		return ProtectedReturn<Output>(&output_, [lock]() { /* Keep a copy of the mutex pointer */ });
	}
	
	/*!
	* \brief Sets input trigger, a function that is called before every execution. Its intended use is to have it load the
	* parametres asynchronously from someplace
	*
	* \param The function, taking a reference to the input as parameter
	*
	* \note Race conditions may occur if the execution is not paused, the trigger itself is run on the same thread as the loop
	*/
	void setInputTrigger(std::function<void(Input &)> trigger)
	{
		inputTrigger_ = trigger;
	}
	
	/*!
	* \brief Sets output trigger, a function that is called after every execution. Its intended use is to have it save the output
	* asynchronously someplace
	*
	* \param The function, taking a const reference to the output as parameter
	*
	* \note Race conditions may occur if the execution is not paused, the trigger itself is run on the same thread as the loop
	*/
	void setOutputTrigger(std::function<void(const Output &)> trigger)
	{
		outputTrigger_ = trigger;
	}
};
#endif // STATE_MACHINE_H
