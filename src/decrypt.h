#pragma once

void GLB_InitSystem(void);
char* GLB_GetItem(int handle);
int GLB_GetItemID(const char* in_name);
void GLB_FreeAll(void);
void GLB_Extract(void);
void GLB_List(void);
void GLB_WriteHeaderFile(void);
void GLB_ConvertItems(void);