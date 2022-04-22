#pragma once
#include <mutex>
#include <atomic>
#include <vector>

template<typename T>
class LinkedQueue
{
	struct Linked
	{
		Linked* next;
		T* value; /* null: it's a new location reader should ignore it, non-null: reader can read but writer should ignore */
	};

	static constexpr std::size_t slot_size = sizeof(T);
	static constexpr std::size_t linked_size = sizeof(Linked);

public:
	LinkedQueue()
	{
		Reset();
	}

	~LinkedQueue()
	{
		Clear();
		std::lock_guard<std::mutex> lock{ write_mutex };
		delete writing;
	}

	bool Push(const T& element)
	{
		return Push_Element(element);
	}

	bool Push(T&& element)
	{
		return Push_Element(std::move(element));
	}

	bool Pop(std::vector<T>& ret_vals)
	{
		std::lock_guard<std::mutex> lock{ read_mutex };

		if (!reading || !reading->value)
		{
			return false;
		}

		Linked* current_reading = reading;
		while (current_reading && current_reading->value)
		{
			ret_vals.push_back(*current_reading->value);

			delete current_reading->value;
			current_reading->value = nullptr;

			Linked* next_reading = current_reading->next;
			delete current_reading;

			current_reading = next_reading;
		}

		/* the next place where we will find the next data it either can be empty or have some value */
		reading = current_reading;
		return true;
	}

	void Clear()
	{
		std::lock_guard<std::mutex> lock_read{ read_mutex };

		Linked* current_reading = reading;
		while (current_reading && current_reading->value)
		{
			delete current_reading->value;
			current_reading->value = nullptr;

			Linked* next_reading = current_reading->next;
			delete current_reading;

			current_reading = next_reading;
		}

		reading = current_reading;
	}

private:
	template <class... _Tys>
	bool Push_Element(_Tys&&... vals)
	{
		Linked* next_writing = new Linked{ 0 };

		if (!next_writing)
		{
			//new memory not available!
			return false;
		}

		std::lock_guard<std::mutex> lock{ write_mutex };

		if (!writing || writing->value)
		{
			//some race condition needs to be fixed!
			return false;
		}

		writing->next = next_writing;
		writing->value = new T(std::forward<_Tys>(vals)...);

		/* pointing to the new memory where we will be writing new data next time */
		writing = next_writing;
		return true;
	}

	void Reset()
	{
		Linked* starting_linked = new Linked{ 0 };

		if (!starting_linked)
		{
			// memory not available!
			return;
		}

		reading = starting_linked;
		writing = starting_linked;
	}

private:
	Linked* reading{ nullptr };
	Linked* writing{ nullptr };

	mutable std::mutex read_mutex;
	mutable std::mutex write_mutex;
};