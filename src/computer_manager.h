#ifndef COMPUTER_MANAGER_H
#define COMPUTER_MANAGER_H

struct HttpClient;

typedef struct _ComputerInfo {
    char id[256];
    char name[256];
    char **printers;
    int printer_count;
} ComputerInfo;

int get_computer_info(struct HttpClient *client, const char *computer_id, ComputerInfo *info);
void free_computer_info(ComputerInfo *info);
int set_computer_name(struct HttpClient *client, const char *computer_id, const char *new_name);
int add_computer(struct HttpClient *client, const char *computer_id, const char *computer_name);
int add_computer_printer(struct HttpClient *client, const char *computer_id, const char *printer_name);
int remove_computer_printer(struct HttpClient *client, const char *computer_id, const char *printer_name);

#endif