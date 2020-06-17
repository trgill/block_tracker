#include <libpq-fe.h>
#include <stdint.h>

#define DB_OK 0
#define DB_INIT_FAILED 1
#define DB_CREATE_ERROR 2
#define DB_FORMAT_ERROR 3
#define DB_QUERY_ERROR 4
#define DB_NOT_FOUND 5
#define DB_CONNECTION_ERROR 5

#define DB_NAME "blkdev_history.db"

int add_system(PGconn *conn, int *system_id);
int get_dbconn_data(char **user, char **password, char **host, char **dbname);
int init_db(PGconn **conn, char *user, char *password, char *host, char *dbname);
int close_db(PGconn *conn);
int update_row(PGconn *conn, int system_id, const char *event, const char *syspath,
               const char *devpath, const char *subsystem, const char *devtype,
               const char *devnode, const char *wwn, const char *serial,
               int major, int minor, uint64_t blkdev_size);
int add_event(PGconn *conn, const char *event, int device_id);
