/**
 ******************************************************************************
 * @file    spark_wiring_version.h
 * @authors Matthew McGowan
 * @date    02 March 2015
 ******************************************************************************
  Copyright (c) 2015 Spark Labs, Inc.  All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************
 */

#ifndef SPARK_WIRING_VERSION_H
#define	SPARK_WIRING_VERSION_H

#include "spark_protocol_functions.h"


struct ApplicationProductID {
    ApplicationProductID(product_id_t id) { 
        spark_protocol_set_product_id(spark_protocol_instance(), id);
    }
};

struct ApplicationProductVersion {
    ApplicationProductVersion(product_firmware_version_t version) { 
        spark_protocol_set_product_firmware_version(spark_protocol_instance(), version);
    }
};

#ifdef PRODUCT_ID
#undef PRODUCT_ID
#endif

#define PRODUCT_ID(x)           ApplicationProductID __appProductID(x);
#define PRODUCT_VERSION(x)       ApplicationProductVersion __appProductVersion(x);


#endif	/* SPARK_WIRING_VERSION_H */
