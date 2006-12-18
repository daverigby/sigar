/*
 * Copyright (C) [2004, 2005, 2006], Hyperic, Inc.
 * This file is part of SIGAR.
 * 
 * SIGAR is free software; you can redistribute it and/or modify
 * it under the terms version 2 of the GNU General Public License as
 * published by the Free Software Foundation. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

#include <jni.h>
#include <stdlib.h>

#include "vmcontrol_wrapper.h"

#ifdef VMCONTROL_WRAPPER_SUPPORTED

#define JENV (*env)

#define VMWARE_PACKAGE "org/hyperic/sigar/vmware/"

#define VMWARE_JNI(m) JNICALL Java_org_hyperic_sigar_vmware_##m

#define VMWARE_FIND_CLASS(name) \
    JENV->FindClass(env, VMWARE_PACKAGE name)

#define VMWARE_CLASS_SIG(name) \
    "L" VMWARE_PACKAGE name ";"

#define VMWARE_EX_SERVER 1
#define VMWARE_EX_VM     2

static void vmware_throw_last_error(JNIEnv *env, void *ptr, int type)
{
    jclass errorClass = VMWARE_FIND_CLASS("VMwareException");
    char *msg;
    int retval;

    switch (type) {
      case VMWARE_EX_SERVER:
        retval = VMControl_ServerGetLastError((VMControlServer *)ptr,
                                              &msg);
        break;
      case VMWARE_EX_VM:
        retval = VMControl_VMGetLastError((VMControlVM *)ptr,
                                          &msg);
        break;
    }

    JENV->ThrowNew(env, errorClass, msg);
    free(msg);
}

#define vmware_throw_last_vm_error() \
    vmware_throw_last_error(env, vm, VMWARE_EX_VM);

#define vmware_throw_last_server_error() \
    vmware_throw_last_error(env, server, VMWARE_EX_SERVER)

static jint vmware_get_pointer(JNIEnv *env, jobject obj)
{
    jclass cls = JENV->GetObjectClass(env, obj);
    jfieldID field = JENV->GetFieldID(env, cls, "ptr", "I");
    return JENV->GetIntField(env, obj, field);
}

#define dVM(obj) \
    VMControlVM *vm = \
       (VMControlVM *)vmware_get_pointer(env, obj)

#define dSERVER(obj) \
    VMControlServer *server = \
       (VMControlServer *)vmware_get_pointer(env, obj)

#define dPARAMS(obj) \
    VMControlConnectParams *params = \
       (VMControlConnectParams *)vmware_get_pointer(env, obj)

JNIEXPORT jboolean VMWARE_JNI(VMwareObject_init)
(JNIEnv *env, jclass classinstance, jstring jlib)
{
    const char *lib;
    int retval;

    if (jlib) {
        lib = JENV->GetStringUTFChars(env, jlib, NULL);
    }
    else {
        lib = getenv("VMCONTROL_SHLIB");
    }

    retval = vmcontrol_wrapper_api_init(lib);

    if (jlib) {
        JENV->ReleaseStringUTFChars(env, jlib, lib);
    }

    if (retval != 0) {
        return JNI_FALSE;
    }

    return VMControl_Init() && VMControl_VMInit();
}

JNIEXPORT jint VMWARE_JNI(ConnectParams_create)
(JNIEnv *env, jclass classinstance,
 jstring jhost, jint port, jstring juser, jstring jpass)
{
    jint ptr;
    const char *host=NULL;
    const char *user=NULL;
    const char *pass=NULL;

    if (jhost) {
        host = JENV->GetStringUTFChars(env, jhost, NULL);
    }
    if (juser) {
        user = JENV->GetStringUTFChars(env, juser, NULL);
    }
    if (jpass) {
        pass = JENV->GetStringUTFChars(env, jpass, NULL);
    }

    ptr = (jint)VMControl_ConnectParamsNew(host, port, user, pass);

    if (host) {
        JENV->ReleaseStringUTFChars(env, jhost, host);
    }
    if (user) {
        JENV->ReleaseStringUTFChars(env, juser, user);
    }
    if (pass) {
        JENV->ReleaseStringUTFChars(env, jpass, pass);
    }

    return ptr;
}

JNIEXPORT void VMWARE_JNI(ConnectParams_destroy)
(JNIEnv *env, jobject obj)
{
    dPARAMS(obj);
    VMControl_ConnectParamsDestroy(params);
}

JNIEXPORT jint VMWARE_JNI(VMwareServer_create)
(JNIEnv *env, jclass classinstance)
{
    return (jint)VMControl_ServerNewEx();
}

JNIEXPORT void VMWARE_JNI(VMwareServer_destroy)
(JNIEnv *env, jclass obj)
{
    dSERVER(obj);
    VMControl_ServerDestroy(server);
}

JNIEXPORT void VMWARE_JNI(VMwareServer_disconnect)
(JNIEnv *env, jclass obj)
{
    dSERVER(obj);
    VMControl_ServerDisconnect(server);
}

JNIEXPORT jboolean VMWARE_JNI(VMwareServer_isConnected)
(JNIEnv *env, jclass obj)
{
    dSERVER(obj);
    return VMControl_ServerIsConnected(server);
}

JNIEXPORT void VMWARE_JNI(VMwareServer_connect)
(JNIEnv *env, jobject obj, jobject params_obj)
{
    dSERVER(obj);
    dPARAMS(params_obj);

    if (!VMControl_ServerConnectEx(server, params)) {
        vmware_throw_last_server_error();
    }
}

JNIEXPORT jboolean VMWARE_JNI(VMwareServer_isRegistered)
(JNIEnv *env, jclass obj, jstring jconfig)
{
    dSERVER(obj);
    const char *config =
        JENV->GetStringUTFChars(env, jconfig, NULL);
    jboolean value;
    jboolean retval =
        VMControl_ServerIsRegistered(server, config, &value);

    JENV->ReleaseStringUTFChars(env, jconfig, config);

    if (!retval) {
        vmware_throw_last_server_error();
        return JNI_FALSE;
    }

    return value;
}

JNIEXPORT jobject VMWARE_JNI(VMwareServer_getRegisteredVmNames)
(JNIEnv *env, jclass obj)
{
    dSERVER(obj);
    char **ptr, **names;
    jobject listobj;
    jclass listclass =
        JENV->FindClass(env, "java/util/ArrayList");
    jmethodID listid =
        JENV->GetMethodID(env, listclass, "<init>", "()V");
    jmethodID addid =
        JENV->GetMethodID(env, listclass, "add",
                          "(Ljava/lang/Object;)Z");

    listobj = JENV->NewObject(env, listclass, listid);

    ptr = names = VMControl_ServerEnumerate(server);

    if (ptr) {
        while (*ptr) {
            JENV->CallBooleanMethod(env, listobj, addid,  
                                    JENV->NewStringUTF(env, *ptr));
            if (JENV->ExceptionOccurred(env)) {
                JENV->ExceptionDescribe(env);
            }
            free(*ptr);
            ptr++;
        }
        free(names);
    }

    return listobj;
}

JNIEXPORT jstring VMWARE_JNI(VMwareServer_getResource)
(JNIEnv *env, jclass obj, jstring jkey)
{
    dSERVER(obj);
    jstring retval;
    const char *key =
        JENV->GetStringUTFChars(env, jkey, NULL);
    char *value = 
        VMControl_ServerGetResource(server, (char *)key);

    JENV->ReleaseStringUTFChars(env, jkey, key);

    if (!value) {
        vmware_throw_last_server_error();
        return NULL;
    }

    retval = JENV->NewStringUTF(env, value);
    free(value);
    return retval;
}

JNIEXPORT jstring VMWARE_JNI(VMwareServer_exec)
(JNIEnv *env, jclass obj, jstring jxml)
{
    dSERVER(obj);
    jstring retval;
    const char *xml =
        JENV->GetStringUTFChars(env, jxml, NULL);
    char *value = 
        VMControl_ServerExec(server, xml);

    JENV->ReleaseStringUTFChars(env, jxml, xml);

    if (!value) {
        vmware_throw_last_server_error();
        return NULL;
    }

    retval = JENV->NewStringUTF(env, value);
    free(value);
    return retval;
}

JNIEXPORT jint VMWARE_JNI(VM_create)
(JNIEnv *env, jclass classinstance)
{
    return (jint)VMControl_VMNewEx();
}

JNIEXPORT void VMWARE_JNI(VM_destroy)
(JNIEnv *env, jclass obj)
{
    dVM(obj);
    VMControl_VMDestroy(vm);
}

JNIEXPORT void VMWARE_JNI(VM_disconnect)
(JNIEnv *env, jclass obj)
{
    dVM(obj);
    VMControl_VMDisconnect(vm);
}

JNIEXPORT jboolean VMWARE_JNI(VM_isConnected)
(JNIEnv *env, jclass obj)
{
    dVM(obj);
    return VMControl_VMIsConnected(vm);
}

JNIEXPORT void VMWARE_JNI(VM_connect)
(JNIEnv *env, jobject obj, jobject params_obj,
 jstring jconfig, jint mks)
{
    dVM(obj);
    dPARAMS(params_obj);
    const char *config =
        JENV->GetStringUTFChars(env, jconfig, NULL);    

    jboolean retval =
        VMControl_VMConnectEx(vm, params, config, mks);

    JENV->ReleaseStringUTFChars(env, jconfig, config);

    if (!retval) {
        vmware_throw_last_vm_error();
    }
}

JNIEXPORT jint VMWARE_JNI(VM_getExecutionState)
(JNIEnv *env, jclass obj)
{
    dVM(obj);
    unsigned int state;

    if (!VMControl_VMGetExecutionState(vm, &state)) {
        vmware_throw_last_vm_error();
        return -1;
    }

    return state;
}

JNIEXPORT jint VMWARE_JNI(VM_getRemoteConnections)
(JNIEnv *env, jclass obj)
{
    dVM(obj);
    unsigned int num;

    if (!VMControl_VMGetRemoteConnections(vm, &num)) {
        vmware_throw_last_vm_error();
        return -1;
    }

    return num;
}

JNIEXPORT jint VMWARE_JNI(VM_getUptime)
(JNIEnv *env, jclass obj)
{
    dVM(obj);
    unsigned int uptime;

    if (!VMControl_VMGetUptime(vm, &uptime)) {
        vmware_throw_last_vm_error();
        return -1;
    }

    return uptime;
}

JNIEXPORT jint VMWARE_JNI(VM_getHeartbeat)
(JNIEnv *env, jclass obj)
{
    dVM(obj);
    unsigned int heartbeat;

    if (!VMControl_VMGetHeartbeat(vm, &heartbeat)) {
        vmware_throw_last_vm_error();
        return -1;
    }

    return heartbeat;
}

JNIEXPORT jint VMWARE_JNI(VM_getToolsLastActive)
(JNIEnv *env, jclass obj)
{
    dVM(obj);
    unsigned int seconds;

    if (!VMControl_VMToolsLastActive(vm, &seconds)) {
        vmware_throw_last_vm_error();
        return -1;
    }

    return seconds;
}

JNIEXPORT jstring VMWARE_JNI(VM_getRunAsUser)
(JNIEnv *env, jclass obj)
{
    dVM(obj);
    char *user;
    jstring juser;

    if (!VMControl_VMGetRunAsUser(vm, &user)) {
        vmware_throw_last_vm_error();
        return NULL;
    }

    juser = JENV->NewStringUTF(env, user);
    free(user);
    return juser;
}

JNIEXPORT jint VMWARE_JNI(VM_getPermissions)
(JNIEnv *env, jclass obj)
{
    dVM(obj);
    unsigned int permissions;

    if (!VMControl_VMGetCapabilities(vm, &permissions)) {
        vmware_throw_last_vm_error();
        return -1;
    }

    return permissions;
}

JNIEXPORT jstring VMWARE_JNI(VM_getConfig)
(JNIEnv *env, jclass obj, jstring jkey)
{
    dVM(obj);
    jstring retval;
    const char *key =
        JENV->GetStringUTFChars(env, jkey, NULL);
    char *value = 
        VMControl_VMGetConfig(vm, (char *)key);

    JENV->ReleaseStringUTFChars(env, jkey, key);

    if (!value) {
        vmware_throw_last_vm_error();
        return NULL;
    }

    retval = JENV->NewStringUTF(env, value);
    free(value);
    return retval;
}

JNIEXPORT void VMWARE_JNI(VM_setConfig)
(JNIEnv *env, jclass obj, jstring jkey, jstring jvalue)
{
    dVM(obj);
    jboolean retval;
    const char *key =
        JENV->GetStringUTFChars(env, jkey, NULL);
    const char *value =
        JENV->GetStringUTFChars(env, jvalue, NULL);

    retval = VMControl_VMSetConfig(vm, (char *)key, (char *)value);

    JENV->ReleaseStringUTFChars(env, jkey, key);
    JENV->ReleaseStringUTFChars(env, jvalue, value);

    if (!retval) {
        vmware_throw_last_vm_error();
    }
}

JNIEXPORT jstring VMWARE_JNI(VM_getResource)
(JNIEnv *env, jclass obj, jstring jkey)
{
    dVM(obj);
    jstring retval;
    const char *key =
        JENV->GetStringUTFChars(env, jkey, NULL);
    char *value = 
        VMControl_VMGetResource(vm, (char *)key);

    JENV->ReleaseStringUTFChars(env, jkey, key);

    if (!value) {
        vmware_throw_last_vm_error();
        return NULL;
    }

    retval = JENV->NewStringUTF(env, value);
    free(value);
    return retval;
}

JNIEXPORT jstring VMWARE_JNI(VM_getGuestInfo)
(JNIEnv *env, jclass obj, jstring jkey)
{
    dVM(obj);
    jstring retval;
    const char *key =
        JENV->GetStringUTFChars(env, jkey, NULL);
    char *value = 
        VMControl_VMGetGuestInfo(vm, (char *)key);

    JENV->ReleaseStringUTFChars(env, jkey, key);

    if (!value) {
        vmware_throw_last_vm_error();
        return NULL;
    }

    retval = JENV->NewStringUTF(env, value);
    free(value);
    return retval;
}

JNIEXPORT void VMWARE_JNI(VM_setGuestInfo)
(JNIEnv *env, jclass obj, jstring jkey, jstring jvalue)
{
    dVM(obj);
    jboolean retval;
    const char *key =
        JENV->GetStringUTFChars(env, jkey, NULL);
    const char *value =
        JENV->GetStringUTFChars(env, jvalue, NULL);

    retval = VMControl_VMSetGuestInfo(vm, (char *)key, (char *)value);

    JENV->ReleaseStringUTFChars(env, jkey, key);
    JENV->ReleaseStringUTFChars(env, jvalue, value);

    if (!retval) {
        vmware_throw_last_vm_error();
    }
}

JNIEXPORT jint VMWARE_JNI(VM_getProductInfo)
(JNIEnv *env, jclass obj, jint type)
{
    dVM(obj);
    unsigned int value;

    if (!VMControl_VMGetProductInfo(vm, type, &value)) {
        vmware_throw_last_vm_error();
        return -1;
    }

    return value;
}

JNIEXPORT void VMWARE_JNI(VM_start)
(JNIEnv *env, jclass obj, jint mode)
{
    dVM(obj);
    
    if (!VMControl_VMStart(vm, mode)) {
        vmware_throw_last_vm_error();
    }                          
}

JNIEXPORT void VMWARE_JNI(VM_stop)
(JNIEnv *env, jclass obj, jint mode)
{
    dVM(obj);
    
    if (!VMControl_VMStopOrReset(vm, 1, mode)) {
        vmware_throw_last_vm_error();
    }                          
}

JNIEXPORT void VMWARE_JNI(VM_reset)
(JNIEnv *env, jclass obj, jint mode)
{
    dVM(obj);
    
    if (!VMControl_VMStopOrReset(vm, 0, mode)) {
        vmware_throw_last_vm_error();
    }                          
}

JNIEXPORT void VMWARE_JNI(VM_suspend)
(JNIEnv *env, jclass obj, jint mode)
{
    dVM(obj);
    
    if (!VMControl_VMSuspendToDisk(vm, mode)) {
        vmware_throw_last_vm_error();
    }                          
}

JNIEXPORT void VMWARE_JNI(VM_createSnapshot)
(JNIEnv *env, jclass obj,
 jstring jname, jstring jdescr,
 jboolean quiesce, jboolean memory)
{
    dVM(obj);
    const char *name =
        JENV->GetStringUTFChars(env, jname, NULL);
    const char *descr =
        JENV->GetStringUTFChars(env, jdescr, NULL);

    if (!VMControl_VMCreateSnapshot(vm, name, descr, quiesce, memory)) {
        vmware_throw_last_vm_error();
    }                          

    JENV->ReleaseStringUTFChars(env, jname, name);
    JENV->ReleaseStringUTFChars(env, jdescr, descr);
}

JNIEXPORT void VMWARE_JNI(VM_revertToSnapshot)
(JNIEnv *env, jclass obj)
{
    dVM(obj);
    
    if (!VMControl_VMRevertToSnapshot(vm)) {
        vmware_throw_last_vm_error();
    }                          
}

JNIEXPORT void VMWARE_JNI(VM_removeAllSnapshots)
(JNIEnv *env, jclass obj)
{
    dVM(obj);
    
    if (!VMControl_VMRemoveAllSnapshots(vm)) {
        vmware_throw_last_vm_error();
    }                          
}

JNIEXPORT jboolean VMWARE_JNI(VM_hasSnapshot)
(JNIEnv *env, jclass obj)
{
    dVM(obj);
    jboolean value;

    if (!VMControl_VMHasSnapshot(vm, &value)) {
        vmware_throw_last_vm_error();
        return JNI_FALSE;
    }

    return value;
}

JNIEXPORT jlong VMWARE_JNI(VM_getPid)
(JNIEnv *env, jclass obj)
{
    dVM(obj);
    unsigned int pid;

    if (!VMControl_VMGetPid(vm, &pid)) {
        vmware_throw_last_vm_error();
        return -1;
    }

    return (jlong)pid;
}

JNIEXPORT jint VMWARE_JNI(VM_getId)
(JNIEnv *env, jclass obj)
{
    dVM(obj);
    unsigned int id;

    if (!VMControl_VMGetId(vm, &id)) {
        vmware_throw_last_vm_error();
        return -1;
    }

    return id;
}

JNIEXPORT void VMWARE_JNI(VM_saveScreenshot)
(JNIEnv *env, jclass obj, jstring jname)
{
    dVM(obj);
    jboolean retval;
    const char *name =
        JENV->GetStringUTFChars(env, jname, NULL);

    retval = VMControl_MKSSaveScreenshot(vm, name, "PNG");

    JENV->ReleaseStringUTFChars(env, jname, name);

    if (!retval) {
        vmware_throw_last_vm_error();
    }
}

JNIEXPORT void VMWARE_JNI(VM_deviceConnect)
(JNIEnv *env, jclass obj, jstring jdevice)
{
    dVM(obj);
    const char *device =
        JENV->GetStringUTFChars(env, jdevice, NULL);
    jboolean retval =
        VMControl_VMDeviceConnect(vm, device);

    JENV->ReleaseStringUTFChars(env, jdevice, device);

    if (!retval) {
        vmware_throw_last_vm_error();
    }
}

JNIEXPORT void VMWARE_JNI(VM_deviceDisconnect)
(JNIEnv *env, jclass obj, jstring jdevice)
{
    dVM(obj);
    const char *device =
        JENV->GetStringUTFChars(env, jdevice, NULL);
    jboolean retval =
        VMControl_VMDeviceDisconnect(vm, device);

    JENV->ReleaseStringUTFChars(env, jdevice, device);

    if (!retval) {
        vmware_throw_last_vm_error();
    }
}

JNIEXPORT jboolean VMWARE_JNI(VM_deviceIsConnected)
(JNIEnv *env, jclass obj, jstring jdevice)
{
    dVM(obj);
    const char *device =
        JENV->GetStringUTFChars(env, jdevice, NULL);
    jboolean isConnected;
    jboolean retval =
        VMControl_VMDeviceIsConnected(vm, device,
                                      &isConnected);

    JENV->ReleaseStringUTFChars(env, jdevice, device);

    if (!retval) {
        vmware_throw_last_vm_error();
        return JNI_FALSE;
    }

    return isConnected;
}

#endif /* VMCONTROL_WRAPPER_SUPPORTED */