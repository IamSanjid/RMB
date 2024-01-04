#pragma once
#include <list>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <queue>

struct Event {
protected:
    virtual ~Event() = default;
};

class HandlerFunctionBase {
public:
    inline void exec(Event& evnt) {
        call(evnt);
    }

    virtual ~HandlerFunctionBase() = default;

private:
    virtual void call(Event& evnt) = 0;
};

template <class EventType>
class FunctionHandler : public HandlerFunctionBase {
public:
    using Function = void (*)(EventType&);

    FunctionHandler(Function function) : function{function} {};

    inline void call(Event& evnt) override {
        (*function)(static_cast<EventType&>(evnt));
    }

    bool compare(const Function& rhs_f) const {
        return rhs_f == function;
    }

private:
    Function function;
};

class EventBus {
private:
    EventBus(){};

public:
    static EventBus& Instance() {
        static EventBus instance;
        return instance;
    }

    EventBus(EventBus const&) = delete;
    void operator=(EventBus const&) = delete;

    template <typename EventType>
    void publish(EventType&& evnt) {
        std::scoped_lock<std::mutex> lock_guard(mutex);

        if (!subscribers.contains(typeid(EventType))) {
            return;
        }

        for (auto& handler : subscribers[typeid(EventType)]) {
            handler->exec(std::forward<EventType&>(evnt));
        }
    }

    template <class EventType>
    void subscribe(void (*function)(EventType&)) {
        std::scoped_lock<std::mutex> lock_guard(mutex);

        if (!subscribers.contains(typeid(EventType))) {
            subscribers[typeid(EventType)] = HandlerList();
        }

        subscribers[typeid(EventType)].push_back(
            std::make_unique<FunctionHandler<EventType>>(function));
    }

private:
    using HandlerList = std::list<std::unique_ptr<HandlerFunctionBase>>;

    std::unordered_map<std::type_index, HandlerList> subscribers;
    mutable std::mutex mutex;
};