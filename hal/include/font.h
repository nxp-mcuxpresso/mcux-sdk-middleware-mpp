/*
 * Copyright 2019-2022 NXP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *  */

/*
 * copied from sdk 2.11.0 folder at: middleware/eiq/common/gprintf/font.h
 * not modified unless copyright header addition
 */

#ifndef __FONT_H__
#define __FONT_H__

#define FONT_XSize  8
#define FONT_YSize  16

/* Character bitmaps for Console 12pt */
static const char consolas_12ptBitmaps[] =
{
/* @0 ' ' */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //

        /* @0 '!' (2 pixels wide) */
        0x00, //
        0x00, //
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x00, //
        0xC0, // ##
        0xC0, // ##
        0x00, //
        0x00, //
        0x00, //

        /* @16 '"' (5 pixels wide) */
        0x00, //
        0x00, //
        0xD8, // ## ##
        0xD8, // ## ##
        0xD8, // ## ##
        0xD8, // ## ##
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //

        /* @32 '#' (8 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x22, //   #   #
        0x22, //   #   #
        0x22, //   #   #
        0xFF, // ########
        0x22, //   #   #
        0x44, //  #   #
        0xFF, // ########
        0x44, //  #   #
        0x44, //  #   #
        0x44, //  #   #
        0x00, //
        0x00, //
        0x00, //

        /* @48 '$' (6 pixels wide) */
        0x00, //
        0x10, //    #
        0x10, //    #
        0x78, //  ####
        0xB0, // # ##
        0xB0, // # ##
        0xA0, // # #
        0x60, //  ##
        0x38, //   ###
        0x24, //   #  #
        0x24, //   #  #
        0x2C, //   # ##
        0xF8, // #####
        0x20, //   #
        0x20, //   #
        0x00, //

        /* @64 '%' (8 pixels wide) */
        0x00, //
        0x00, //
        0x61, //  ##    #
        0x92, // #  #  #
        0x96, // #  # ##
        0x94, // #  # #
        0x68, //  ## #
        0x18, //    ##
        0x16, //    # ##
        0x39, //   ###  #
        0x29, //   # #  #
        0x49, //  #  #  #
        0xC6, // ##   ##
        0x00, //
        0x00, //
        0x00, //

        /* @80 '&' (8 pixels wide) */
        0x00, //
        0x00, //
        0x38, //   ###
        0x44, //  #   #
        0x44, //  #   #
        0x44, //  #   #
        0x68, //  ## #
        0x30, //   ##
        0x7A, //  #### #
        0x8A, // #   # #
        0x8E, // #   ###
        0xC6, // ##   ##
        0x7B, //  #### ##
        0x00, //
        0x00, //
        0x00, //

        /* @96 ''' (2 pixels wide) */
        0x00, //
        0x00, //
        0xC0, // ##
        0xC0, // ##
        0xC0, // ##
        0xC0, // ##
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //

        /* @112 '(' (4 pixels wide) */
        0x00, //
        0x10, //    #
        0x20, //   #
        0x60, //  ##
        0x40, //  #
        0xC0, // ##
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0xC0, // ##
        0x40, //  #
        0x60, //  ##
        0x20, //   #
        0x10, //    #

        /* @128 ')' (4 pixels wide) */
        0x00, //
        0x80, // #
        0x40, //  #
        0x60, //  ##
        0x20, //   #
        0x30, //   ##
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x30, //   ##
        0x20, //   #
        0x60, //  ##
        0x40, //  #
        0x80, // #

        /* @144 '*' (7 pixels wide) */
        0x00, //
        0x00, //
        0x10, //    #
        0x92, // #  #  #
        0x7C, //  #####
        0x10, //    #
        0x7C, //  #####
        0x92, // #  #  #
        0x10, //    #
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //

        /* @160 '+' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0xFE, // #######
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x00, //
        0x00, //
        0x00, //
        0x00, //

        /* @176 ',' (4 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x70, //  ###
        0x70, //  ###
        0x30, //   ##
        0x60, //  ##
        0xC0, // ##

        /* @192 '-' (5 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0xF8, // #####
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //

        /* @208 '.' (3 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0xE0, // ###
        0xE0, // ###
        0x00, //
        0x00, //
        0x00, //

        /* @224 '/' (6 pixels wide) */
        0x00, //
        0x00, //
        0x04, //      #
        0x0C, //     ##
        0x08, //     #
        0x08, //     #
        0x18, //    ##
        0x10, //    #
        0x30, //   ##
        0x20, //   #
        0x20, //   #
        0x60, //  ##
        0x40, //  #
        0x40, //  #
        0x80, // #
        0x00, //

        /* @240 '0' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x38, //   ###
        0x44, //  #   #
        0x82, // #     #
        0x86, // #    ##
        0x9A, // #  ## #
        0xB2, // # ##  #
        0xC2, // ##    #
        0x82, // #     #
        0x44, //  #   #
        0x38, //   ###
        0x00, //
        0x00, //
        0x00, //

        /* @256 '1' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x30, //   ##
        0x50, //  # #
        0x90, // #  #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0xFE, // #######
        0x00, //
        0x00, //
        0x00, //

        /* @272 '2' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x78, //  ####
        0x8C, // #   ##
        0x04, //      #
        0x04, //      #
        0x04, //      #
        0x08, //     #
        0x10, //    #
        0x20, //   #
        0x40, //  #
        0xFE, // #######
        0x00, //
        0x00, //
        0x00, //

        /* @288 '3' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0xF8, // #####
        0x04, //      #
        0x04, //      #
        0x0C, //     ##
        0x78, //  ####
        0x0C, //     ##
        0x04, //      #
        0x04, //      #
        0x08, //     #
        0xF0, // ####
        0x00, //
        0x00, //
        0x00, //

        /* @304 '4' (8 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x0C, //     ##
        0x1C, //    ###
        0x14, //    # #
        0x34, //   ## #
        0x24, //   #  #
        0x44, //  #   #
        0xC4, // ##   #
        0xFF, // ########
        0x04, //      #
        0x04, //      #
        0x00, //
        0x00, //
        0x00, //

        /* @320 '5' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0xF8, // #####
        0x80, // #
        0x80, // #
        0x80, // #
        0xF8, // #####
        0x0C, //     ##
        0x04, //      #
        0x04, //      #
        0x08, //     #
        0xF0, // ####
        0x00, //
        0x00, //
        0x00, //

        /* @336 '6' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x3C, //   ####
        0x60, //  ##
        0x40, //  #
        0x80, // #
        0xBC, // # ####
        0xC6, // ##   ##
        0x82, // #     #
        0x82, // #     #
        0x46, //  #   ##
        0x38, //   ###
        0x00, //
        0x00, //
        0x00, //

        /* @352 '7' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0xFE, // #######
        0x06, //      ##
        0x04, //      #
        0x0C, //     ##
        0x08, //     #
        0x18, //    ##
        0x10, //    #
        0x30, //   ##
        0x20, //   #
        0x60, //  ##
        0x00, //
        0x00, //
        0x00, //

        /* @368 '8' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x78, //  ####
        0x84, // #    #
        0x84, // #    #
        0xCC, // ##  ##
        0x30, //   ##
        0x48, //  #  #
        0x84, // #    #
        0x84, // #    #
        0xC4, // ##   #
        0x78, //  ####
        0x00, //
        0x00, //
        0x00, //

        /* @384 '9' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x38, //   ###
        0xC4, // ##   #
        0x82, // #     #
        0x82, // #     #
        0xC6, // ##   ##
        0x7A, //  #### #
        0x02, //       #
        0x04, //      #
        0x0C, //     ##
        0x70, //  ###
        0x00, //
        0x00, //
        0x00, //

        /* @400 ':' (2 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0xC0, // ##
        0xC0, // ##
        0x00, //
        0x00, //
        0x00, //
        0xC0, // ##
        0xC0, // ##
        0x00, //
        0x00, //
        0x00, //

        /* @416 ';' (4 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x60, //  ##
        0x60, //  ##
        0x00, //
        0x00, //
        0x00, //
        0x70, //  ###
        0x70, //  ###
        0x30, //   ##
        0x60, //  ##
        0xC0, // ##

        /* @432 '<' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x04, //      #
        0x18, //    ##
        0x30, //   ##
        0x60, //  ##
        0xC0, // ##
        0x60, //  ##
        0x30, //   ##
        0x18, //    ##
        0x04, //      #
        0x00, //
        0x00, //
        0x00, //

        /* @448 '=' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0xFE, // #######
        0x00, //
        0x00, //
        0xFE, // #######
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //

        /* @464 '>' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x80, // #
        0x60, //  ##
        0x30, //   ##
        0x18, //    ##
        0x0C, //     ##
        0x18, //    ##
        0x30, //   ##
        0x60, //  ##
        0x80, // #
        0x00, //
        0x00, //
        0x00, //

        /* @480 '?' (4 pixels wide) */
        0x00, //
        0x00, //
        0xC0, // ##
        0x20, //   #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0xE0, // ###
        0x80, // #
        0x80, // #
        0x00, //
        0xC0, // ##
        0xC0, // ##
        0x00, //
        0x00, //
        0x00, //

        /* @496 '@' (9 pixels wide) */
        0x00, 0x00, 0x1C, //    ###
        0x22, //   #   #
        0x41, //  #     #
        0x41, //  #     #
        0x9D, // #  ### #
        0xB5, // # ## # #
        0xA5, // # #  # #
        0xA5, // # #  # #
        0xA5, // # #  # #
        0xBE, // # #####
        0x80, // #
        0x44, //  #   #
        0x3C, //   ####
        0x00,

        /* @528 'A' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x38, //   ###
        0x28, //   # #
        0x28, //   # #
        0x2C, //   # ##
        0x44, //  #   #
        0x44, //  #   #
        0x44, //  #   #
        0xFE, // #######
        0x82, // #     #
        0x82, // #     #
        0x00, //
        0x00, //
        0x00, //

        /* @544 'B' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0xF8, // #####
        0x84, // #    #
        0x84, // #    #
        0x8C, // #   ##
        0xF8, // #####
        0x8C, // #   ##
        0x84, // #    #
        0x84, // #    #
        0x8C, // #   ##
        0xF8, // #####
        0x00, //
        0x00, //
        0x00, //

        /* @560 'C' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x3C, //   ####
        0x62, //  ##   #
        0x40, //  #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0xC0, // ##
        0x42, //  #    #
        0x3C, //   ####
        0x00, //
        0x00, //
        0x00, //

        /* @576 'D' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0xF8, // #####
        0x84, // #    #
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x86, // #    ##
        0x8C, // #   ##
        0xF8, // #####
        0x00, //
        0x00, //
        0x00, //

        /* @592 'E' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0xFC, // ######
        0x80, // #
        0x80, // #
        0x80, // #
        0xFC, // ######
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0xFC, // ######
        0x00, //
        0x00, //
        0x00, //

        /* @608 'F' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0xFC, // ######
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0xFC, // ######
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x00, //
        0x00, //
        0x00, //

        /* @624 'G' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x3C, //   ####
        0x62, //  ##   #
        0x40, //  #
        0x80, // #
        0x80, // #
        0x8E, // #   ###
        0x82, // #     #
        0xC2, // ##    #
        0x62, //  ##   #
        0x3E, //   #####
        0x00, //
        0x00, //
        0x00, //

        /* @640 'H' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0xFE, // #######
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x00, //
        0x00, //
        0x00, //

        /* @656 'I' (5 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0xF8, // #####
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0xF8, // #####
        0x00, //
        0x00, //
        0x00, //

        /* @672 'J' (5 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0xF8, // #####
        0x08, //     #
        0x08, //     #
        0x08, //     #
        0x08, //     #
        0x08, //     #
        0x08, //     #
        0x08, //     #
        0x88, // #   #
        0x70, //  ###
        0x00, //
        0x00, //
        0x00, //

        /* @688 'K' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x84, // #    #
        0x88, // #   #
        0x98, // #  ##
        0xB0, // # ##
        0xE0, // ###
        0xE0, // ###
        0xB0, // # ##
        0x98, // #  ##
        0x88, // #   #
        0x84, // #    #
        0x00, //
        0x00, //
        0x00, //

        /* @704 'L' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0xFC, // ######
        0x00, //
        0x00, //
        0x00, //

        /* @13 'M' (7 pixels wide) */
        0x00, 0x00, 0x00, 0x46, //  #   ##
        0xCA, // ##  # #
        0xAA, // # # # #
        0xAA, // # # # #
        0xB2, // # ##  #
        0xB2, // # ##  #
        0xB2, // # ##  #
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x00, //
        0x00, //
        0x00, //

        /* @752 'N' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0xC2, // ##    #
        0xC2, // ##    #
        0xA2, // # #   #
        0xA2, // # #   #
        0x92, // #  #  #
        0x92, // #  #  #
        0x8A, // #   # #
        0x8A, // #   # #
        0x86, // #    ##
        0x86, // #    ##
        0x00, //
        0x00, //
        0x00, //

        /* @768 'O' (8 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x3C, //   ####
        0x42, //  #    #
        0xC1, // ##     #
        0x81, // #      #
        0x81, // #      #
        0x81, // #      #
        0x81, // #      #
        0x83, // #     ##
        0x42, //  #    #
        0x3C, //   ####
        0x00, //
        0x00, //
        0x00, //

        /* @784 'P' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0xF8, // #####
        0x8C, // #   ##
        0x84, // #    #
        0x84, // #    #
        0x8C, // #   ##
        0xF0, // ####
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x00, //
        0x00, //
        0x00, //

        /* @800 'Q' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x38, //   ###
        0x44, //  #   #
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x44, //  #   #
        0x38, //   ###
        0x10, //    #
        0x12, //    #  #
        0x0E, //     ###

        /* @816 'R' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0xF8, // #####
        0x84, // #    #
        0x84, // #    #
        0x8C, // #   ##
        0xF0, // ####
        0x90, // #  #
        0x88, // #   #
        0x88, // #   #
        0x84, // #    #
        0x86, // #    ##
        0x00, //
        0x00, //
        0x00, //

        /* @832 'S' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x78, //  ####
        0x80, // #
        0x80, // #
        0x80, // #
        0x70, //  ###
        0x18, //    ##
        0x04, //      #
        0x04, //      #
        0x04, //      #
        0xF8, // #####
        0x00, //
        0x00, //
        0x00, //

        /* @848 'T' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0xFE, // #######
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x00, //
        0x00, //
        0x00, //

        /* @864 'U' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0xC4, // ##   #
        0x78, //  ####
        0x00, //
        0x00, //
        0x00, //

        /* @880 'V' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x82, // #     #
        0x82, // #     #
        0xC6, // ##   ##
        0x44, //  #   #
        0x44, //  #   #
        0x6C, //  ## ##
        0x68, //  ## #
        0x28, //   # #
        0x28, //   # #
        0x38, //   ###
        0x00, //
        0x00, //
        0x00, //

        /* @896 'W' (8 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x81, // #      #
        0x81, // #      #
        0x81, // #      #
        0x89, // #   #  #
        0x99, // #  ##  #
        0x95, // #  # # #
        0xB5, // # ## # #
        0xA5, // # #  # #
        0xE3, // ###   ##
        0xC3, // ##    ##
        0x00, //
        0x00, //
        0x00, //

        /* @912 'X' (9 pixels wide) */
        0x00,  //
        0x00,  //
        0x00,  //
        0xC3,  // ##    ##
        0x66,  //  ##  ##
        0x34,  //   ## #
        0x1C,  //    ###
        0x18,  //    ##
        0x1C,  //    ###
        0x34,  //   ## #
        0x26,  //   #  ##
        0x63,  //  ##   ##
        0xC1,  // ##     ##
        0x00,  //
        0x00,  //
        0x00,  //

        /* @944 'Y' (9 pixels wide) */
        0x00,  //
        0x00, //
        0x00,  //
        0x80,  // #       #
        0x41,  //  #     #
        0x22,  //   #   #
        0x36, //   ## ##
        0x14,  //    # #
        0x08,  //     #
        0x08,  //     #
        0x08,  //     #
        0x08, //     #
        0x08,  //     #
        0x00,  //
        0x00,  //
        0x00, //

        /* @976 'Z' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0xFE, // #######
        0x04, //      #
        0x0C, //     ##
        0x08, //     #
        0x10, //    #
        0x10, //    #
        0x20, //   #
        0x60, //  ##
        0x40, //  #
        0xFE, // #######
        0x00, //
        0x00, //
        0x00, //

        /* @992 '[' (4 pixels wide) */
        0x00, //
        0x00, //
        0xF0, // ####
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0xF0, // ####

        /* @1008 '\' (6 pixels wide) */
        0x00, //
        0x00, //
        0x80, // #
        0x40, //  #
        0x40, //  #
        0x60, //  ##
        0x20, //   #
        0x20, //   #
        0x30, //   ##
        0x10, //    #
        0x18, //    ##
        0x08, //     #
        0x08, //     #
        0x0C, //     ##
        0x04, //      #
        0x00, //

        /* @1024 ']' (4 pixels wide) */
        0x00, //
        0x00, //
        0xF0, // ####
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0xF0, // ####

        /* @1040 '^' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x30, //   ##
        0x50, //  # #
        0x48, //  #  #
        0x8C, // #   ##
        0x84, // #    #
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //

        /* @1056 '_' (9 pixels wide) */
        0x00,  //
        0x00,  //
        0x00,  //
        0x00,  //
        0x00,  //
        0x00,  //
        0x00,  //
        0x00,  //
        0x00,  //
        0x00,  //
        0x00,  //
        0x00,  //
        0x00,  //
        0x00,  //
        0x00,  //
        0xFF,  // #########

        /* @1088 '`' (3 pixels wide) */
        0x00, //
        0x00, //
        0xC0, // ##
        0x60, //  ##
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //

        /* @1104 'a' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x38, //   ###
        0x44, //  #   #
        0x04, //      #
        0x7C, //  #####
        0x84, // #    #
        0x8C, // #   ##
        0x74, //  ### #
        0x00, //
        0x00, //
        0x00, //

        /* @1120 'b' (6 pixels wide) */
        0x00, //
        0x00, //
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0xB8, // # ###
        0xCC, // ##  ##
        0x84, // #    #
        0x84, // #    #
        0x84, // #    #
        0x88, // #   #
        0xF0, // ####
        0x00, //
        0x00, //
        0x00, //

        /* @1136 'c' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x3C, //   ####
        0x40, //  #
        0x80, // #
        0x80, // #
        0x80, // #
        0xC0, // ##
        0x3C, //   ####
        0x00, //
        0x00, //
        0x00, //

        /* @1152 'd' (6 pixels wide) */
        0x00, //
        0x00, //
        0x04, //      #
        0x04, //      #
        0x04, //      #
        0x04, //      #
        0x3C, //   ####
        0x44, //  #   #
        0x84, // #    #
        0x84, // #    #
        0x84, // #    #
        0xCC, // ##  ##
        0x74, //  ### #
        0x00, //
        0x00, //
        0x00, //

        /* @1168 'e' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x3C, //   ####
        0x46, //  #   ##
        0x82, // #     #
        0xFE, // #######
        0x80, // #
        0xC0, // ##
        0x3E, //   #####
        0x00, //
        0x00, //
        0x00, //

        /* @1184 'f' (7 pixels wide) */
        0x00, //
        0x00, //
        0x1E, //    ####
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0xFC, // ######
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x00, //
        0x00, //
        0x00, //

        /* @1200 'g' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x7E, //  ######
        0x84, // #    #
        0x84, // #    #
        0x84, // #    #
        0xF8, // #####
        0x80, // #
        0xFC, // ######
        0x82, // #     #
        0x82, // #     #
        0x7C, //  #####

        /* @1216 'h' (6 pixels wide) */
        0x00, //
        0x00, //
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0xB8, // # ###
        0xC4, // ##   #
        0x84, // #    #
        0x84, // #    #
        0x84, // #    #
        0x84, // #    #
        0x84, // #    #
        0x00, //
        0x00, //
        0x00, //

        /* @1232 'i' (6 pixels wide) */
        0x00, //
        0x00, //
        0x60, //  ##
        0x60, //  ##
        0x00, //
        0x00, //
        0xE0, // ###
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0xFC, // ######
        0x00, //
        0x00, //
        0x00, //

        /* @1248 'j' (5 pixels wide) */
        0x00, //
        0x00, //
        0x18, //    ##
        0x18, //    ##
        0x00, //
        0x00, //
        0xF8, // #####
        0x08, //     #
        0x08, //     #
        0x08, //     #
        0x08, //     #
        0x08, //     #
        0x08, //     #
        0x08, //     #
        0x98, // #  ##
        0x70, //  ###

        /* @1264 'k' (6 pixels wide) */
        0x00, //
        0x00, //
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x8C, // #   ##
        0x90, // #  #
        0xE0, // ###
        0xA0, // # #
        0x90, // #  #
        0x88, // #   #
        0x84, // #    #
        0x00, //
        0x00, //
        0x00, //

        /* @1280 'l' (6 pixels wide) */
        0x00, //
        0x00, //
        0xE0, // ###
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0xFC, // ######
        0x00, //
        0x00, //
        0x00, //

        /* @1296 'm' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0xFE, // #######
        0xDA, // ## ## #
        0x92, // #  #  #
        0x92, // #  #  #
        0x92, // #  #  #
        0x92, // #  #  #
        0x92, // #  #  #
        0x00, //
        0x00, //
        0x00, //

        /* @1312 'n' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0xB8, // # ###
        0xC4, // ##   #
        0x84, // #    #
        0x84, // #    #
        0x84, // #    #
        0x84, // #    #
        0x84, // #    #
        0x00, //
        0x00, //
        0x00, //

        /* @1328 'o' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x3C, //   ####
        0x46, //  #   ##
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0xC4, // ##   #
        0x78, //  ####
        0x00, //
        0x00, //
        0x00, //

        /* @1344 'p' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0xBC, // # ####
        0xC6, // ##   ##
        0x82, // #     #
        0x82, // #     #
        0x82, // #     #
        0x84, // #    #
        0xF8, // #####
        0x80, // #
        0x80, // #
        0x80, // #

        /* @1360 'q' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x3C, //   ####
        0x44, //  #   #
        0x84, // #    #
        0x84, // #    #
        0x84, // #    #
        0xCC, // ##  ##
        0x74, //  ### #
        0x04, //      #
        0x04, //      #
        0x04, //      #

        /* @1376 'r' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0xB8, // # ###
        0xC4, // ##   #
        0x84, // #    #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x00, //
        0x00, //
        0x00, //

        /* @1392 's' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x78, //  ####
        0x80, // #
        0xC0, // ##
        0x78, //  ####
        0x04, //      #
        0x04, //      #
        0xF8, // #####
        0x00, //
        0x00, //
        0x00, //

        /* @1408 't' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0xFE, // #######
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x30, //   ##
        0x1E, //    ####
        0x00, //
        0x00, //
        0x00, //

        /* @1424 'u' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x84, // #    #
        0x84, // #    #
        0x84, // #    #
        0x84, // #    #
        0x84, // #    #
        0x8C, // #   ##
        0x74, //  ### #
        0x00, //
        0x00, //
        0x00, //

        /* @1440 'v' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x82, // #     #
        0xC6, // ##   ##
        0x44, //  #   #
        0x44, //  #   #
        0x28, //   # #
        0x28, //   # #
        0x30, //   ##
        0x00, //
        0x00, //
        0x00, //

        /* @1456 'w' (7 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x82, // #     #
        0x82, // #     #
        0x92, // #  #  #
        0xAA, // # # # #
        0xAA, // # # # #
        0xAA, // # # # #
        0xC4, // ##   #
        0x00, //
        0x00, //
        0x00, //

        /* @1472 'x' (8 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0xC6, // ##   ##
        0x6C, //  ## ##
        0x38, //   ###
        0x18, //    ##
        0x2C, //   # ##
        0x66, //  ##  ##
        0xC3, // ##    ##
        0x00, //
        0x00, //
        0x00, //

        /* @1488 'y' (8 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x41, //  #     #
        0x63, //  ##   ##
        0x22, //   #   #
        0x26, //   #  ##
        0x14, //    # #
        0x14, //    # #
        0x18, //    ##
        0x18, //    ##
        0x10, //    #
        0xE0, // ###

        /* @1504 'z' (6 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0xFC, // ######
        0x08, //     #
        0x10, //    #
        0x30, //   ##
        0x20, //   #
        0x40, //  #
        0xFC, // ######
        0x00, //
        0x00, //
        0x00, //

        /* @1520 '{' (6 pixels wide) */
        0x00, //
        0x00, //
        0x1C, //    ###
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0xC0, // ##
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x20, //   #
        0x30, //   ##
        0x1C, //    ###

        /* @1536 '|' (1 pixels wide) */
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #
        0x80, // #

        /* @1552 '}' (6 pixels wide) */
        0x00, //
        0x00, //
        0xE0, // ###
        0x30, //   ##
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x0C, //     ##
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x10, //    #
        0x30, //   ##
        0xE0, // ###

        /* @1568 '~' (8 pixels wide) */
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x71, //  ###   #
        0x99, // #  ##  #
        0x8E, // #   ###
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //
        0x00, //

        };
#endif
