/* 
 * File:   AbstractMemory.h
 * Author: wayne
 *
 * Created on August 28, 2008, 5:52 PM
 */

#ifndef _ABSTRACTMEMORY_H
#define	_ABSTRACTMEMORY_H

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

    typedef struct {
        caddr_t address;
        long size;
    } AbstractMemory;


#ifdef	__cplusplus
}
#endif

#endif	/* _ABSTRACTMEMORY_H */

