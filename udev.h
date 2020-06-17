#include <libpq-fe.h>

int init_udev(struct udev **udev);
int init_database_rows(PGconn *db, int system_id, struct udev *udev);
int monitor_events(PGconn *db, struct udev *udev, int system_id);

#define UDEV_OK 0
#define UDEV_INIT_FAILED 100
#define UDEV_NULL_PARAM 101
#define UDEV_ENUMERATE_CONTEXT_FAILED 102
#define UDEV_GET_LIST_FAILED 102
#define UDEV_DB_UPDATE_FAILED 102