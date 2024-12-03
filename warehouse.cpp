#include <array>
#include <mutex>
#include <ctime>
#include <vector>
#include <thread>
#include <string>
#include <cstdlib>
#include <iostream>
#include <condition_variable>

// Define warehouse size
#ifndef WSIZE
#define WSIZE 8
#elif (WSIZE < 8)
#error WSIZE shall be at least 8
#endif

class Vehicle
{
    int id;
    std::string type;
    std::string model;

public:
    Vehicle(const std::string &_type, const std::string &_model) : type{_type}, model{_model}
    {
        static int serial{1001};
        id = serial++;
    }

    int getID() const { return id; }

    std::string getType() const { return type; }

    std::string getModel() const { return model; }

    virtual void print() const = 0; // Pure virtual function

    virtual ~Vehicle() = default;
};

class Car : public Vehicle
{
    int maxPassengers;

public:
    Car(const std::string &model, int passengers) : Vehicle("Car", model), maxPassengers{passengers} {}

    void print() const override
    {
        std::cout << "\nVehicle ID: " << getID() << "\n Model: " << getModel()
                  << "\n Type: " << getType()
                  << "\n Number of Passengers " << maxPassengers << "\n";
    }
};

class Truck : public Vehicle
{
    float maxLoadWeight;

public:
    Truck(const std::string &model, float loadWeight) : Vehicle("Truck", model), maxLoadWeight{loadWeight} {}

    void print() const override
    {
        std::cout << " \nVehicle ID: " << getID() << "\n Model: " << getModel()
                  << "\n Type: " << getType()
                  << "\n Max Load Weight: " << maxLoadWeight << "\n";
    }
};

// Thread-safe circular buffer (Warehouse)
class Warehouse
{
    std::array<Vehicle *, WSIZE> buffer_;
    size_t head_, tail_, size_;

    std::mutex mtx_;
    std::condition_variable cv_;

public:
    explicit Warehouse() : head_(0), tail_(0), size_(0) {}

    // UNCOPYABLE CIRCULAR BUFFER
    Warehouse(const Warehouse &) = delete;
    Warehouse &operator=(const Warehouse &) = delete;

    // Add a vehicle to the buffer /* PUSH */
    void produce(Vehicle *vehicle)
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]()
                 { return size_ < WSIZE; });

        buffer_[tail_] = vehicle;
        tail_ = (tail_ + 1) % WSIZE;
        size_++;

        cv_.notify_one();
    }

    // Remove a vehicle from the buffer /*POP */
    Vehicle *consume()
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]()
                 { return size_ > 0; });

        Vehicle *vehicle = buffer_[head_];
        head_ = (head_ + 1) % WSIZE;
        size_--;

        cv_.notify_one();
        return vehicle;
    }
};

// Producer thread function
void produceVehicles(Warehouse &buffer)
{
    while (true)
    {
        Vehicle *vehicle;

        if ((std::rand() % 2) == 0)
        {
            vehicle = new Car("Model-C", 4 + std::rand() % 4);
        }
        else
        {
            vehicle = new Truck("Model-T", 25.06 + (std::rand() % 100));
        }

        buffer.produce(vehicle);

        std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % 500));
    }
}

// Consumer thread function
void consumeVehicles(Warehouse &buffer, int id)
{
    std::mutex omtx;

    while (true)
    {
        Vehicle *vehicle = buffer.consume();

        omtx.lock();
        std::cout << " \n -------------CONSUMER ------------ " << "\n ";
        vehicle->print();
        delete vehicle;
        omtx.unlock();

        std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % 1000));
    }
}

// Main function
int main(int argc, const char *argv[])
{
    std::srand(std::time(nullptr));

    Warehouse warehouse;

    int numConsumers = std::atoi(argv[1]);
    if (argc < 2)
    {
        std::cerr << " Argument should be greater than 2\n";
        return 1;
    }
    if (numConsumers < 2)
    {
        std::cout << "Number of consumers must be at least 2.\n";
        return 1;
    }

    // Start producer thread
    std::thread producer(produceVehicles, std::ref(warehouse));
    std::cout << " \n PRODUCED:" << WSIZE << " \n";

    std::vector<std::thread> consumers;

    for (int i = 0; i < numConsumers; ++i)
    {
        consumers.emplace_back(consumeVehicles, std::ref(warehouse), i + 1);
    }

    producer.join();

    for (auto &consumer : consumers)
    {
        consumer.join();
    }

    return 0;
}
