/****************************************************************************/
/*              Beebem - (c) David Alan Gilbert 1994                        */
/*              ------------------------------------                        */
/* This program may be distributed freely within the following restrictions:*/
/*                                                                          */
/* 1) You may not charge for this program or for any part of it.            */
/* 2) This copyright message must be distributed with all copies.           */
/* 3) This program must be distributed complete with source code.  Binary   */
/*    only distribution is not permitted.                                   */
/* 4) The author offers no warrenties, or guarentees etc. - you use it at   */
/*    your own risk.  If it messes something up or destroys your computer   */
/*    thats YOUR problem.                                                   */
/* 5) You may use small sections of code from this program in your own      */
/*    applications - but you must acknowledge its use.  If you plan to use  */
/*    large sections then please ask the author.                            */
/*                                                                          */
/* If you do not agree with any of the above then please do not use this    */
/* program.                                                                 */
/* Please report any problems to the author at beebem@treblig.org           */
/****************************************************************************/
char *mode7fontRaw[12*9] = {
"            x     x x       xx     xxx    xx        x       x        x     x        x                                           ",
"            x     x x      x  x   x x x   xx  x    x x      x       x       x     x x x     x                                 x ",
"            x     x x      x      x x        x     x x      x      x         x     xxx      x                                x  ",
"            x             xxx      xxx      x       x              x         x      x     xxxxx           xxxxx             x   ",
"            x              x        x x    x       x x x           x         x     xxx      x                              x    ",
"                           x      x x x   x  xx    x  x             x       x     x x x     x       x               x     x     ",
"            x             xxxxx    xxx       xx     xx x             x     x        x               x               x           ",
"                                                                                                   x                            ",
"                                                                                                                                ",
/*
 01234567012345670123456701234567012345670123456701234567012345670123456701234567012345670123456701234567012345670123456701234567*/
"    x       x      xxx    xxxxx      x    xxxxx     xx    xxxxx    xxx     xxx                        x            x       xxx  ",
"   x x     xx     x   x       x     xx    x        x          x   x   x   x   x                      x              x     x   x ",
"  x   x     x         x      x     x x    xxxx    x          x    x   x   x   x     x       x       x     xxxxx      x       x  ",
"  x   x     x       xx      xx    x  x        x   xxxx      x      xxx     xxxx                    x                  x     x   ",
"  x   x     x      x          x   xxxxx       x   x   x    x      x   x       x                     x     xxxxx      x      x   ",
"   x x      x     x       x   x      x    x   x   x   x    x      x   x      x                       x              x           ",
"    x      xxx    xxxxx    xxx       x     xxx     xxx     x       xxx     xx       x       x         x            x        x   ",
"                                                                                            x                                   ",
"                                                                                           x                                    ",

"   xxx      x     xxxx     xxx    xxxx    xxxxx   xxxxx    xxx    x   x    xxx        x   x   x   x       x   x   x   x    xxx  ",
"  x   x    x x    x   x   x   x   x   x   x       x       x   x   x   x     x         x   x  x    x       xx xx   x   x   x   x ",
"  x xxx   x   x   x   x   x       x   x   x       x       x       x   x     x         x   x x     x       x x x   xx  x   x   x ",
"  x x x   xxxxx   xxxx    x       x   x   xxxx    xxxx    x       xxxxx     x         x   xx      x       x x x   x x x   x   x ",
"  x xxx   x   x   x   x   x       x   x   x       x       x  xx   x   x     x         x   x x     x       x   x   x  xx   x   x ",
"  x       x   x   x   x   x   x   x   x   x       x       x   x   x   x     x     x   x   x  x    x       x   x   x   x   x   x ",
"   xxx    x   x   xxxx     xxx    xxxx    xxxxx   x        xxx    x   x    xxx     xxx    x   x   xxxxx   x   x   x   x    xxx  ",
"                                                                                                                                ",
"                                                                                                                                ",

"  xxxx     xxx    xxxx     xxx    xxxxx   x   x   x   x   x   x   x   x   x   x   xxxxx           x                        x x  ",
"  x   x   x   x   x   x   x   x     x     x   x   x   x   x   x   x   x   x   x       x     x     x         x       x      x x  ",
"  x   x   x   x   x   x   x         x     x   x   x   x   x   x    x x     x x       x     x      x          x     xxx    xxxxx ",
"  xxxx    x   x   xxxx     xxx      x     x   x    x x    x x x     x       x       x     xxxxx   x       xxxxx   x x x    x x  ",
"  x       x x x   x x         x     x     x   x    x x    x x x    x x      x      x       x      x xx       x      x     xxxxx ",
"  x       x  x    x  x    x   x     x     x   x     x     x x x   x   x     x     x         x         x     x       x      x x  ",
"  x        xx x   x   x    xxx      x      xxx      x      x x    x   x     x     xxxxx              x                     x x  ",
"                                                                                                    x                           ",
"                                                                                                    xxx                         ",

"                  x                   x              x            x         x       x     x        xx                           ",
"                  x                   x             x             x                       x         x                           ",
"           xxx    xxxx     xxxxx   xxxx    xxx      x      xxxx   xxxx     xx       x     x   x     x     xx x    xxxx     xxx  ",
"  xxxxx       x   x   x   x       x   x   x   x    xxx    x   x   x   x     x       x     x  x      x     x x x   x   x   x   x ",
"           xxxx   x   x   x       x   x   xxxxx     x     x   x   x   x     x       x     xxx       x     x x x   x   x   x   x ",
"          x   x   x   x   x       x   x   x         x     x   x   x   x     x       x     x  x      x     x x x   x   x   x   x ",
"           xxxx   xxxx     xxxxx   xxxx    xxx      x      xxxx   x   x    xxx      x     x   x    xxx    x   x   x   x    xxx  ",
"                                                              x                     x                                           ",
"                                                           xxx                     x                                            ",

"                                    x                                                       x      x x    xx                    ",
"                                    x                                                       x      x x      x       x     xxxxx ",
"  xxxx     xxxx    x xx     xxxx   xxx    x   x   x   x   x   x   x   x   x   x   xxxxx     x      x x    xx              xxxxx ",
"  x   x   x   x    xx      x        x     x   x   x   x   x   x    x x    x   x      x      x      x x      x     xxxxx   xxxxx ",
"  x   x   x   x    x        xxx     x     x   x    x x    x x x     x     x   x     x       x  x   x x    xx   x          xxxxx ",
"  x   x   x   x    x           x    x     x   x    x x    x x x    x x    x   x    x          xx   x x        xx    x     xxxxx ",
"  xxxx     xxxx    x       xxxx      x     xxxx     x      x x    x   x    xxxx   xxxxx      x x   x x       x x          xxxxx ",
"  x           x                                                               x              xxx   x x       xxx          xxxxx ",
"  x           x                                                            xxx                 x               x                ",

"        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@",
"        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@",
"        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@",
"                                @@@@    @@@@    @@@@    @@@@        @@@@    @@@@    @@@@    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
"                                @@@@    @@@@    @@@@    @@@@        @@@@    @@@@    @@@@    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
"                                @@@@    @@@@    @@@@    @@@@        @@@@    @@@@    @@@@    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
"                                                                                                                                ",
"                                                                                                                                ",
"                                                                                                                                ",

"        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@",
"        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@",
"        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@",
"                                @@@@    @@@@    @@@@    @@@@        @@@@    @@@@    @@@@    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
"                                @@@@    @@@@    @@@@    @@@@        @@@@    @@@@    @@@@    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
"                                @@@@    @@@@    @@@@    @@@@        @@@@    @@@@    @@@@    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
"@@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    ",
"@@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    ",
"@@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    ",

"   xxx      x     xxxxx   xxxxx    x xx  xxxxxxx   xxx   xxxxxxx  xxxxx   xxxxx                      xx           xx      xxxx  ",
"  xx xx    xx    xx   xx xx   xx   x xx  xx       xx     xx   xx xx   xx xx   xx                    xx             xx    xx  xx ",
" xx  xxx  xxx         xx      xx  xx xx  xxxxx   xx           xx xx   xx xx   xx   xx      xx      xx     xxxxx     xx       xx ",
" xx x xx   xx      xxxx    xxxx  xx  xx      xx  xxxxxx     xx    xxxxx   xxxxxx   xx      xx     xx                 xx     xx  ",
" xxx  xx   xx     xx          xx xxxxxxx      xx xx   xx   xx    xx   xx      xx                   xx     xxxxx     xx     xx   ",
"  xx xx    xx    xx      xx   xx     xx  xx  xx  xx   xx   xx    xx   xx     xx    xx      xx       xx             xx           ",
"   xxx    xxxx   xxxxxxx  xxxxx      xx   xxxx    xxxxx    xx     xxxxx    xxx     xx      xx        xx           xx       xx   ",
"                                                                                            x                                   ",
"                                                                                           x                                    ",

"  xxxxx    xxx   xxxxxx    xxxx  xxxxx   xxxxxxx xxxxxxx   xxxx  xx   xx  xxxx      xxxx xx   xx xx      xx   xx xx   xx   xxx  ",
" xx   xx  xx xx  xx   xx  xx  xx xx  xx  xx      xx       xx  xx xx   xx   xx         xx xx  xx  xx      xxx xxx xxx  xx  xx xx ",
" xx xxxx xx   xx xx   xx xx      xx   xx xx      xx      xx      xx   xx   xx         xx xx xx   xx      xxxxxxx xxxx xx xx   xx",
" xx x xx xxxxxxx xxxxxx  xx      xx   xx xxxxx   xxxx    xx xxxx xxxxxxx   xx         xx xxxx    xx      xx x xx xx xxxx xx   xx",
" xx xxx  xx   xx xx   xx xx      xx   xx xx      xx      xx   xx xx   xx   xx    xx   xx xx xx   xx      xx   xx xx  xxx xx   xx",
" xx      xx   xx xx   xx  xx  xx xx  xx  xx      xx       xx  xx xx   xx   xx    xx  xx  xx  xx  xx      xx   xx xx   xx  xx xx ",
"  xxxx   xx   xx xxxxxx    xxxx  xxxxx   xxxxxxx xx        xxx x xx   xx  xxxx    xxxx   xx   xx xxxxxxx xx   xx xx   xx   xxx  ",
"                                                                                                                                ",
"                                                                                                                                ",

"        @@@@        @@@@@@@@@@@@@@@@    @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@",
"        @@@@        @@@@@@@@@@@@@@@@    @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@",
"        @@@@        @@@@@@@@@@@@@@@@    @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@",
"                                    @@@@@@@@    @@@@    @@@@        @@@@    @@@@    @@@@    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
"                                    @@@@@@@@    @@@@    @@@@        @@@@    @@@@    @@@@    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
"                                    @@@@@@@@    @@@@    @@@@        @@@@    @@@@    @@@@    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
"    @@@@    @@@@    @@@@    @@@@            @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@",
"    @@@@    @@@@    @@@@    @@@@            @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@",
"    @@@@    @@@@    @@@@    @@@@            @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@    @@@@",

"        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@",
"        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@",
"        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@        @@@@        @@@@@@@@@@@@",
"                                @@@@    @@@@    @@@@    @@@@        @@@@    @@@@    @@@@    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
"                                @@@@    @@@@    @@@@    @@@@        @@@@    @@@@    @@@@    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
"                                @@@@    @@@@    @@@@    @@@@        @@@@    @@@@    @@@@    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"};

