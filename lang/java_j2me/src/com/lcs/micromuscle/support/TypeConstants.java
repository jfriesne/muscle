/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */
package com.meyer.micromuscle.support;

/** Java declarations for Be's type constants.  The values are all the same as Be's. */
public interface TypeConstants
{
   public static final int B_ANY_TYPE     = 1095653716; // 'ANYT',  // wild card
   public static final int B_BOOL_TYPE    = 1112493900; // 'BOOL',
   public static final int B_DOUBLE_TYPE  = 1145195589; // 'DBLE',
   public static final int B_FLOAT_TYPE   = 1179406164; // 'FLOT',
   public static final int B_INT64_TYPE   = 1280069191; // 'LLNG',  // a.k.a. long in Java
   public static final int B_INT32_TYPE   = 1280265799; // 'LONG',  // a.k.a. int in Java
   public static final int B_INT16_TYPE   = 1397248596; // 'SHRT',  // a.k.a. short in Java
   public static final int B_INT8_TYPE    = 1113150533; // 'BYTE',  // a.k.a. byte in Java
   public static final int B_MESSAGE_TYPE = 1297303367; // 'MSGG',
   public static final int B_POINTER_TYPE = 1347310674; // 'PNTR',  // parsed as int in Java (but not very useful)
   public static final int B_POINT_TYPE   = 1112559188; // 'BPNT',  // muscle.support.Point in Java
   public static final int B_RECT_TYPE    = 1380270932; // 'RECT',  // muscle.support.Rect in Java
   public static final int B_STRING_TYPE  = 1129534546; // 'CSTR',  // java.lang.String
   public static final int B_OBJECT_TYPE  = 1330664530; // 'OPTR',  
   public static final int B_RAW_TYPE     = 1380013908; // 'RAWT',  
   public static final int B_MIME_TYPE    = 1296649541; // 'MIME',  
}

