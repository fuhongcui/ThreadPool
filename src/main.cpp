#include "ThreadPool/ThreadPool.h"
#include <iostream>

unsigned int WorkTask(unsigned int value)
{
    unsigned int totalValue = 0;
    for(unsigned int i = 0; i < value + 1; ++i)
    {
        totalValue +=i;
    }
    return totalValue;
}
class WorkObject
{
public:
    int Work(int i)
    {
        for(int j = 0; j < i; ++j)
        {
            std::cout << j << "\n";
        }
        return 100;
    }
};


int main()
{
    ThreadPool threadPool;

    WorkObject obj1;
    threadPool.CommitTask(&WorkObject::Work, &obj1, 999999);
    WorkObject ob2;
    threadPool.CommitTask(&WorkObject::Work, &ob2, 999999);


    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    threadPool.Start();
    threadPool.Stop();


    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Time taken: " << duration.count() << " milliseconds" << std::endl;
    return 0;
}
