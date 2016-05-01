#ifndef __NBA_KNAPP_DEFS_HH__
#define __NBA_KNAPP_DEFS_HH__

/* Behavioral limits. */
#define KNAPP_MAX_KERNEL_ARGS (16)
#define KNAPP_SCIF_MAX_CONN_RETRY (5)
#define KNAPP_OFFLOAD_CTRLBUF_SIZE (32)

/* Hardware limits. */
#define KNAPP_MAX_CORES_PER_DEVICE (60)
#define KNAPP_MAX_LCORES_PER_DEVICE (240)

/* Base numbers. */
#define KNAPP_HOST_DATAPORT_BASE (2000)
#define KNAPP_HOST_CTRLPORT_BASE (2100)
#define KNAPP_MIC_DATAPORT_BASE (2200)
#define KNAPP_MIC_CTRLPORT_BASE (2300)


#endif //__NBA_KNAPP_DEFS_HH__

// vim: ts=8 sts=4 sw=4 et
