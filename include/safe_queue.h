#include <queue>
#include <mutex>
#include <utility>
#include <condition_variable>

template <class T>
class SafeQueue{
private:
	std::queue<T> q;
	mutable std::mutex m;
	std::condition_variable c;
    
public:
	SafeQueue(): q(), m(), c(){}
	~SafeQueue(){}

	void push(T t){
		std::lock_guard<std::mutex> lock(m);
		q.push(t);
		c.notify_one();
	}

	bool empty(){
		std::lock_guard<std::mutex> lock(m);
		return q.empty();
	}

	T pop(){
		std::unique_lock<std::mutex> lock(m);
		while(q.empty()){
			c.wait(lock);
		}
		T val = q.front();
		q.pop();
		return val;
	}
};