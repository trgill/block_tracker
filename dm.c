#include <libdevmapper.h>
#include <gmodule.h>
#include <inttypes.h>

int get_devices(GSList **device_list)
{
    struct dm_names *names;
    unsigned next = 0;
    struct dm_task *dmt;
    int r = 0;

    *device_list = g_slist_alloc();
    if (!(dmt = dm_task_create(DM_DEVICE_LIST)))
        return 0;

    if (!dm_task_run(dmt))
        goto out;

    if (!(names = dm_task_get_names(dmt)))
        goto out;

    if (!names->dev)
        goto out;

    do
    {
        names = (struct dm_names *)((char *)names + next);
        printf("Insert : %s\n", names->name);
        *device_list = g_slist_append(*device_list, strdup(names->name));

        next = names->next;
    } while (next);
out:
    dm_task_destroy(dmt);
    return r;
}

int get_tables(GSList *device_list)
{
    int rc = 0;
    struct dm_task *dm_task;
    struct dm_info info;
    GSList *iterator = NULL;
    char *name = NULL;

    for (iterator = device_list; iterator; iterator = iterator->next)
    {
        name = iterator->data;

        if (name == NULL || strlen(name) == 0)
            continue;
        printf("name = %s\n", name);

        if (!(dm_task = dm_task_create(DM_DEVICE_TABLE)))
        {
            printf("Failed to create DM task");
            return -1;
        }

        if (!dm_task_set_name(dm_task, name))
        {
            printf("dm_task_set_name failed!");
            return -1;
        }

        if (!dm_task_run(dm_task))
            goto out;

        if (!dm_task_get_info(dm_task, &info))
            goto out;

        if (!info.exists)
        {
            printf("Device does not exist.");
            goto out;
        }
        printf("name = %s major = %d minor = %d\n", name, info.major, info.minor);

        void *next = NULL;
        uint64_t start;
        uint64_t length_part;
        uint64_t length = 0;
        char *type;
        char *parameters;
        do
        {
            next = dm_get_next_target(dm_task, next, &start, &length_part, &type,
                                      &parameters);
            length += length_part;
            printf("start = %" PRIu64 " length = %" PRIu64 " type = %s params = %s\n", start, length, type, parameters);
        } while (next);
    }
out:
    return rc;
}