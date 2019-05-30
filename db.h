#include <sqlite3.h>
#include <stdint.h>

#define DB_OK 0
#define DB_INIT_FAILED 1
#define DB_CREATE_ERROR 2
#define DB_FORMAT_ERROR 3
#define DB_QUERY_ERROR 4

#define DB_NAME "blkdev_history.db"

int init_db(sqlite3 **db, int *sqlite_rc);
int update_row(sqlite3 *db, const char *event, const char *syspath,
               const char *devpath, const char *subsystem, const char *devtype,
               const char *devnode, const char *wwn, const char *serial,
               int major, int minor, uint64_t blkdev_size);
int add_event(sqlite3 *db, const char *event, const char *devnode);
