#ifndef PRINTER_MANAGER_H
#define PRINTER_MANAGER_H

#include <windows.h>

typedef struct _LocalPrinterInfo {
    char name[256];
    wchar_t wname[256];
    char port[256];
    char driver[256];
    int enabled;
} LocalPrinterInfo;

typedef struct _PrinterList {
    LocalPrinterInfo *printers;
    int count;
} PrinterList;

int enum_local_printers(PrinterList *list);
void free_printer_list(PrinterList *list);

#endif