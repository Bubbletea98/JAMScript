
#include "jam.h"
#include "core.h"

#include <strings.h>
#include <pthread.h>


jactivity_t *jam_rexec_async(jamstate_t *js, char *aname, char *fmask, ...)
{
    va_list args;
    nvoid_t *nv;
    int i = 0;
    arg_t *qargs;

    assert(fmask != NULL);    

    if (strlen(fmask) > 0)
        qargs = (arg_t *)calloc(strlen(fmask), sizeof(arg_t));
    else
        qargs = NULL;
        
    cbor_item_t *arr = cbor_new_indefinite_array();
    cbor_item_t *elem;

    va_start(args, fmask);

    while(*fmask)
    {
        elem = NULL;
        switch(*fmask++)
        {
            case 'n':
                nv = va_arg(args, nvoid_t*);
                elem = cbor_build_bytestring(nv->data, nv->len);
                qargs[i].val.nval = nv;
                qargs[i].type = NVOID_TYPE;
                break;
            case 's':
                qargs[i].val.sval = strdup(va_arg(args, char *));
                qargs[i].type = STRING_TYPE;
                elem = cbor_build_string(qargs[i].val.sval);
                break;
            case 'i':
                qargs[i].val.ival = va_arg(args, int);
                qargs[i].type = INT_TYPE;
                elem = cbor_build_uint32(abs(qargs[i].val.ival));
                if (qargs[i].val.ival < 0)
                    cbor_mark_negint(elem);
                break;
            case 'd':
            case 'f':
                qargs[i].val.dval = va_arg(args, double);
                qargs[i].type = DOUBLE_TYPE;
                elem = cbor_build_float8(qargs[i].val.dval);
                break;
            default:
                break;
        }
        i++;
        if (elem != NULL)
            assert(cbor_array_push(arr, elem) == true);
    }
    va_end(args);

    // Need to add start to activity_new()
    jactivity_t *jact = activity_new(js->atable, aname);
  
    command_t *cmd = command_new_using_cbor("REXEC", "ASY", aname, jact->actid, js->cstate->conf->device_id, arr, qargs, i);
    
    temprecord_t *trec = jam_newtemprecord(js, jact, cmd);
    taskcreate(jam_rexec_run_wrapper, trec, STACKSIZE);

    return jact;
}


void jam_rexec_run_wrapper(void *arg)
{
    temprecord_t *trec = (temprecord_t *)arg;
    
    jam_async_runner((jamstate_t *)trec->arg1, (jactivity_t *)trec->arg2, (command_t *)trec->arg3);
    free(trec);
}


void jam_async_runner(jamstate_t *js, jactivity_t *jact, command_t *cmd)
{
    command_t *rcmd;

    // The protocol for REXEC processing is still evolving.. it is without
    // retries at this point. May be with nanomsg we don't need retries at the
    // RPC level. This is something we need to investigate closely by looking at
    // the reliability model that is provided by nanonmsg.
    
    // Send the command.. and wait for reply..
    queue_enq(jact->outq, cmd, sizeof(command_t));
    task_wait(jact->sem);
    
    nvoid_t *nv = queue_deq(jact->inq);
    if (nv == NULL) 
    {
        jact->state = EXEC_ERROR;
        return;        
    }
    rcmd = (command_t *)nv->data;
    free(nv);
        
    if (rcmd == NULL) 
    {
        jact->state = EXEC_ERROR;
        return;
    }
        
    // What is the reply.. positive ACK and negative problem in that case 
    // stick the error code in the activity
    if (strcmp(rcmd->cmd, "REXEC-ACK") == 0)
    {
        jact->code = command_arg_clone(&(rcmd->args[0]));
        jact->state = EXEC_STARTED; 
    }
    else
    if (strcmp(rcmd->cmd, "REXEC-NAK") == 0) {
        jact->code = command_arg_clone(&(rcmd->args[0]));
        jact->state = EXEC_ABORTED;
    }
}
