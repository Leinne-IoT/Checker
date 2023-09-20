#include <queue>
#include <mutex>
#include <utility>
#include <condition_variable>

template<class T>
class SafeQueueNew{
private:
	std::queue<T> q;

	std::mutex mtx;
	std::condition_variable cv;

	int sync_counter = 0;
	bool finish_processing = false;
	std::condition_variable sync_wait;

	void decreaseSyncCounter(){
		if(--sync_counter == 0){
			sync_wait.notify_one();
		}
	}

public:
	typedef typename std::queue<T>::size_type size_type;

	SafeQueueNew(){}

	~SafeQueueNew(){
		finish();
	}

	void push(T&& item){
		std::lock_guard<std::mutex> lock(mtx);

		q.push(std::move(item));
		cv.notify_one();
	}

	size_type size(){
		std::lock_guard<std::mutex> lock(mtx);
		return q.size();
	}

    bool empty(){
		std::lock_guard<std::mutex> lock(mtx);
        return q.empty();
    }

	[[nodiscard]]
	bool pop(T& item){
		std::lock_guard<std::mutex> lock(mtx);
		if(q.empty()){
			return false;
		}

		item = std::move(q.front());
		q.pop();
		return true;

	}

	[[nodiscard]]
	bool popSync(T& item){
		std::unique_lock<std::mutex> lock(mtx);

		sync_counter++;
		cv.wait(lock, [&] {
			return !q.empty() || finish_processing;
		});

		if(q.empty()){
			decreaseSyncCounter();
			return false;
		}

		item = std::move(q.front());
		q.pop();
		decreaseSyncCounter();
		return true;

	}

	void finish(){
		std::unique_lock<std::mutex> lock(mtx);

		finish_processing = true;
		cv.notify_all();

		sync_wait.wait(lock, [&](){
			return sync_counter == 0;
		});
		finish_processing = false;
	}
};

#include <queue>
#include <mutex>
#include <condition_variable>

// A threadsafe-queue.
template <class T>
class SafeQueue{
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

private:
	std::queue<T> q;
	mutable std::mutex m;
	std::condition_variable c;
};