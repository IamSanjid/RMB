#pragma once
#include <list>
#include <unordered_map>
#include <typeindex>

// stolen from: https://medium.com/@savas/nomad-game-engine-part-7-the-event-system-45a809ccb68f

struct Event
{
protected:
    virtual ~Event() = default;
};

class HandlerFunctionBase {
public:
    void exec(Event* evnt)
    {
        call(evnt);
    }
private:
    virtual void call(Event* evnt) = 0;
};

template<class EventType>
class FunctionHandler : public HandlerFunctionBase
{
public:
    typedef void (*Function)(EventType*);

    FunctionHandler(Function function) : function{ function } {};

    void call(Event* evnt)
    {
        (*function)(static_cast<EventType*>(evnt));
    }

    bool compare(const Function& rhs_f) const
    {
        return rhs_f == function;
    }
private:
    Function function;
};

template<class T, class EventType>
class MemberFunctionHandler : public HandlerFunctionBase
{
public:
    typedef void (T::* MemberFunction)(EventType*);

    MemberFunctionHandler(T* instance, MemberFunction memberFunction) : instance{ instance }, memberFunction{ memberFunction } {};

    void call(Event* evnt)
    {
        (instance->*memberFunction)(static_cast<EventType*>(evnt));
    }

    bool compare(const T* rhs_i, const MemberFunction& rhs_f) const
    {
        return rhs_i == instance && rhs_f == memberFunction;
    }
private:
    T* instance;
    MemberFunction memberFunction;
};

typedef std::list<HandlerFunctionBase*> HandlerList;
class EventBus {
private:
    EventBus() {};
public:
    static EventBus& Instance()
    {
        static EventBus instance;
        return instance;
    }

    EventBus(EventBus const&) = delete;
    void operator=(EventBus const&) = delete;

    template<typename EventType>
    void publish(EventType* evnt)
    {
        HandlerList* handlers = subscribers[typeid(EventType)];

        if (handlers == nullptr) {
            return;
        }

        for (auto& handler : *handlers) {
            if (handler != nullptr) {
                handler->exec(evnt);
            }
        }
    }

    template<class EventType>
    void subscribe(void (*function)(EventType*))
    {
        HandlerList* handlers = subscribers[typeid(EventType)];
        if (handlers == nullptr) {
            handlers = new HandlerList();
            subscribers[typeid(EventType)] = handlers;
        }

        handlers->push_back(new FunctionHandler<EventType>(function));
    }

    template<class T, class EventType>
    void subscribe(T* instance, void (T::* memberFunction)(EventType*))
    {
        HandlerList* handlers = subscribers[typeid(EventType)];
        if (handlers == nullptr) {
            handlers = new HandlerList();
            subscribers[typeid(EventType)] = handlers;
        }

        handlers->push_back(new MemberFunctionHandler<T, EventType>(instance, memberFunction));
    }

    template<class EventType>
    void unsubscribe(void (*function)(EventType*))
    {
        HandlerList* handlers = subscribers[typeid(EventType)];
        if (handlers == nullptr) return;

        for (auto hIt = handlers->begin(); hIt != handlers->end(); ++hIt)
        {
            if (static_cast<FunctionHandler<EventType>*>(*hIt)->compare(function))
            {
                handlers->erase(hIt);
                return;
            }
        }
    }

    template<class T, class EventType>
    void unsubscribe(T* instance, void (T::* memberFunction)(EventType*))
    {
        HandlerList* handlers = subscribers[typeid(EventType)];
        if (handlers == nullptr) return;

        for (auto hIt = handlers->begin(); hIt != handlers->end(); ++hIt)
        {
            if (static_cast<MemberFunctionHandler<T, EventType>*>(*hIt)->compare(instance, memberFunction))
            {
                handlers->erase(hIt);
                return;
            }
        }
    }

    template<class EventType>
    void unsubscribe_all()
    {
        HandlerList* handlers = subscribers[typeid(EventType)];
        if (handlers == nullptr) return;
        handlers->clear();
    }
private:
    std::unordered_map<std::type_index, HandlerList*> subscribers;
};