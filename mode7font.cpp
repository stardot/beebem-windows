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
"           xxx   xx xx     xxxx   xxxxx  xx   xx   xxx     xx       xx     xx                                                 xx",
"          xxxx   xx xx    xx  xx xx x xx xx  xx   xx xx    xx      xx       xx    x x x    xx                                xx ",
"          xxx    x  x     xx     xx x       xx    xx xx   xx      xx         xx    xxx     xx                               xx  ",
"          xx              xxxx    xxxxx    xx      xx             xx         xx  xxxxxxx xxxxxx           xxxxx            xx   ",
"                          xx        x xx  xx     xx xx x          xx         xx    xxx     xx                             xx    ",
"         xxx              xx     xx x xx xx  xx  xx  xx            xx       xx    x x x    xx      xx              xx    xx     ",
"         xxx             xxxxxxx  xxxxx xx   xx   xxx xx            xx     xx                      xx              xx   xx      ",
"                                                                                                  xx                            ",
"                                                                                                                                ",

/*
01234567012345670123456701234567012345670123456701234567012345670123456701234567012345670123456701234567012345670123456701234567*/
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

" xxxxx     xxx   xxxxx    xxxxx  xxxxxx  xx   xx xx   xx xx   xx xx   xx xx  xx  xxxxxxx         xx                xx     xx xx ",
" xx  xx   xx xx  xx  xx  xx   xx   xx    xx   xx xx   xx xx   xx  xx xx  xx  xx      xx    x     xx          x    xxxx    xx xx ",
" xx   xx xx   xx xx   xx  xx       xx    xx   xx xx   xx xx   xx   xxx   xx  xx     xx    xx     xx          xx  xxxxxx  xxxxxxx",
" xx  xx  xx   xx xx  xx    xxx     xx    xx   xx xx   xx xx x xx   xxx    xxxx     xx    xxxxxxx xx xxx  xxxxxxx   xx     xx xx ",
" xxxxx   xx   xx xxxxx       xx    xx    xx   xx  xx xx  xxxxxxx  xx xx    xx     xx      xx          xx     xx    xx    xxxxxxx",
" xx       xx xx  xx  xx  xx   xx   xx    xx   xx  xx xx  xxx xxx xx   xx   xx    xx        x         xx      x     xx     xx xx ",
" xx        xxx   xx   xx  xxxxx    xx     xxxxx    xxx   xx   xx xx   xx   xx    xxxxxxx            xx             xx     xx xx ",
"             xxx                                                                                    xxxx                        ",
"                                                                                                                                ",

"                 xx                   xx           xxxx          xx        xx       xx   xx       xxx                           ",
"                 xx                   xx          xx  xx         xx                      xx        xx                           ",
"          xxxxx  xx xxx   xxxxx   xxx xx  xxxxx   xx      xxxx x xxxxxx   xxx      xxx   xx   xx   xx    xxx xx  xx xxx   xxxxx ",
"  xxxx        xx xxx  xx xx   xx xx  xxx xx   xx xxxxx   xx  xxx xx   xx   xx       xx   xx  xx    xx    xx x xx xxx  xx xx   xx",
"          xxxxxx xx   xx xx      xx   xx xxxxxxx  xx     xx   xx xx   xx   xx       xx   xxxxx     xx    xx x xx xx   xx xx   xx",
"         xx   xx xxx  xx xx   xx xx  xxx xx       xx     xx  xxx xx   xx   xx       xx   xx  xx    xx    xx x xx xx   xx xx   xx",
"          xxxxxx xx xxx   xxxxx   xxx xx  xxxxx   xx      xxx xx xx   xx  xxxx      xx   xx   xx  xxxx   xx   xx xx   xx  xxxxx ",
"                                                              xx                    xx                                          ",
"                                                          xxxxx                  xxxx                                           ",

"                                   xx                                                     xx      xx xx  xxx                    ",
"                                   xx                                                     xx      xx xx    xx       x    xxxxxx ",
" xx xxx   xxx xx xx xxx   xxxxx   xxxx   xx   xx xx   xx xx   xx xx   xx xx   xx xxxxxxx  xx      xx xx   xx             xxxxxx ",
" xxx  xx xx  xxx xxx  xx xx        xx    xx   xx xx   xx xx x xx  xx xx  xx   xx     xx   xx  xx  xx xx    xx xx xxxxxxx xxxxxx ",
" xx   xx xx   xx xx       xxxxx    xx    xx   xx  xx xx  xx x xx   xxx   xx   xx   xxx       xxx  xx xx  xxx xxx         xxxxxx ",
" xxx  xx xx  xxx xx           xx   xx    xx  xxx  xx xx  xx x xx  xx xx  xx  xxx xx         x xx  xx xx     x xx    x    xxxxxx ",
" xx xxx   xxx xx xx       xxxxx     xxx   xxx xx   xxx     x xxx xx   xx  xxx xx xxxxxxx   xxxxx  xx xx    xxxxx         xxxxxx ",
" xx           xx                                                              xx              xx  xx xx       xx         xxxxxx ",
" xx          xxx                                                          xxxxx                                                 ",

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

