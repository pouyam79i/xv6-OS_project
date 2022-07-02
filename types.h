typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;

struct time_data {
  uint ctime;                  // creation time
  uint tt;                     // termination time
  uint ru_t;                   // running time
  uint re_t;                   // ready time
  uint st;                     // sleeping time
};