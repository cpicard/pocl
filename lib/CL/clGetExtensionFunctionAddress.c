#include "pocl_cl.h"

#include <string.h>

/* Note - this is deprecated in 1.1, but (some of) the ICD loaders are built
 * against OCL 1.1, so we need it.
 */ 
CL_API_ENTRY void * CL_API_CALL 
clGetExtensionFunctionAddress(const char * func_name ) 
CL_EXT_SUFFIX__VERSION_1_0
{

#ifdef BUILD_ICD
  if( strcmp(func_name, "clIcdGetPlatformIDsKHR")==0 )
    return (void *)&clIcdGetPlatformIDsKHR;
#endif
  
  return NULL;
}

