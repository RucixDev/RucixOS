#include "driver.h"
#include "console.h"

static void suspend_device(struct device *dev) {
    if (dev->driver && dev->driver->suspend) {
        kprint_str("[PM] Suspending ");
        kprint_str(dev->name);
        kprint_newline();
        dev->driver->suspend(dev);
    }
}

static void resume_device(struct device *dev) {
    if (dev->driver && dev->driver->resume) {
        kprint_str("[PM] Resuming ");
        kprint_str(dev->name);
        kprint_newline();
        dev->driver->resume(dev);
    }
}

void pm_suspend(void) {
    kprint_str("[PM] System Suspend...\n");
    device_for_each(suspend_device);
    kprint_str("[PM] System Suspended.\n");
}

void pm_resume(void) {
    kprint_str("[PM] System Resume...\n");
    device_for_each(resume_device);
    kprint_str("[PM] System Resumed.\n");
}
