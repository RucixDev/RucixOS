#include "drm/drm.h"
#include "heap.h"
#include "string.h"
#include "console.h"

struct drm_device *drm_dev_alloc(struct drm_driver *driver, struct device *parent) {
    (void)parent;
    struct drm_device *dev = (struct drm_device*)kmalloc(sizeof(struct drm_device));
    if (!dev) return 0;
    
    memset(dev, 0, sizeof(struct drm_device));
    dev->driver = driver;
    
    INIT_LIST_HEAD(&dev->crtc_list);
    INIT_LIST_HEAD(&dev->connector_list);
    INIT_LIST_HEAD(&dev->encoder_list);
    INIT_LIST_HEAD(&dev->fb_list);
    
    return dev;
}

int drm_dev_register(struct drm_device *dev, unsigned long flags) {
    if (!dev || !dev->driver) return -1;
    
    kprint_str("[DRM] Registering device: ");
    kprint_str(dev->driver->name);
    kprint_newline();
    
    if (dev->driver->load) {
        return dev->driver->load(dev, flags);
    }
    return 0;
}

void drm_dev_unregister(struct drm_device *dev) {
    if (dev->driver->unload) {
        dev->driver->unload(dev);
    }
     
}

void drm_connector_init(struct drm_device *dev, struct drm_connector *connector, const struct drm_connector_funcs *funcs) {
    connector->dev = dev;
    connector->funcs = funcs;
    INIT_LIST_HEAD(&connector->modes);
    list_add_tail(&connector->list, &dev->connector_list);
}

void drm_encoder_init(struct drm_device *dev, struct drm_encoder *encoder, const struct drm_encoder_funcs *funcs) {
    encoder->dev = dev;
    encoder->funcs = funcs;
    list_add_tail(&encoder->list, &dev->encoder_list);
}

void drm_crtc_init(struct drm_device *dev, struct drm_crtc *crtc, const struct drm_crtc_funcs *funcs) {
    crtc->dev = dev;
    crtc->funcs = funcs;
    list_add_tail(&crtc->list, &dev->crtc_list);
}
