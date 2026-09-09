#pragma once
#include <atomic>
class BoxMenuDisplay;
class BoxOta {
public:
    void Start(BoxMenuDisplay* display);

private:
    std::atomic<bool> started_{false};
    BoxMenuDisplay* display_ = nullptr;
    void Run();
    bool Check();
};
