#pragma once
#define NULL ((void*)0)
static void *memset(void *p,int c,unsigned long long n){volatile unsigned char *q=p;while(n--)*q++=(unsigned char)c;return p;}
