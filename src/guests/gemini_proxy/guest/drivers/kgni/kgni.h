
struct kgni_device {
        uint32_t                devnum;         /* device minor number */
        struct device           *class_dev;
        struct pci_dev          *pdev;
        struct cdev             cdev;
        uint32_t                ref_num;        /* number of references */
};
typedef struct kgni_device kgni_device_t;
/* Private state associated with an open file. */
typedef struct kgni_file {
    kgni_device_t       *device;
    uint64_t            app_data;
} kgni_file_t;
