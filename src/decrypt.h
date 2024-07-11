#pragma once

void GLB_InitSystem(void);
char* GLB_GetItem(int handle);
int GLB_GetItemID(const char* in_name);
int GLB_ItemSize(int handle);
void GLB_FreeAll(void);
void GLB_Extract(void);
void GLB_List(void);
void GLB_WriteHeaderFile(void);
void GLB_ConvertItems(void);