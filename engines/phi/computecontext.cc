#include "computecontext.hh"
#include "computedevice.hh"
#include "../../lib/common.hh"
#include <rte_common.h>

using namespace std;
using namespace nba;

struct phi_event_context {
    ComputeContext *computectx;
    void (*callback)(ComputeContext *ctx, void *user_arg);
    void *user_arg;
};

PhiComputeContext::PhiComputeContext(unsigned ctx_id, ComputeDevice *mother_device)
 : ComputeContext(ctx_id, mother_device)
{
    type_name = "phi";
    size_t mem_size = 8 * 1024 * 1024; // TODO: read from config
    cl_int err_ret;
    PhiComputeDevice *modevice = (PhiComputeDevice *) mother_device;
    clqueue = clCreateCommandQueue(modevice->clctx, modevice->cldevid, 0, &err_ret);
    if (err_ret != CL_SUCCESS) {
        rte_panic("clCreateCommandQueue()@PhiComputeContext() failed\n");
    }
    dev_mempool_in  = new PhiMemoryPool(modevice->clctx, clqueue, HOST_TO_DEVICE),
    dev_mempool_out = new PhiMemoryPool(modevice->clctx, clqueue, DEVICE_TO_HOST),
    cpu_mempool_in  = new CPUMemoryPool();
    cpu_mempool_out = new CPUMemoryPool();
    checkbits_d.clmem = clCreateBuffer(modevice->clctx, CL_MEM_READ_WRITE,
                                       MAX_BLOCKS, NULL, &err_ret);
    if (err_ret != CL_SUCCESS) {
        rte_panic("clCreateBuffer()@PhiComputeContext() failed to create the checkbit region!\n");
    }
    checkbits_h = clEnqueueMapBuffer(clqueue, checkbits_d.clmem, CL_TRUE,
                                     CL_MAP_READ | CL_MAP_WRITE,
                                     0, MAX_BLOCKS, NULL, NULL, &err_ret);
    if (err_ret != CL_SUCCESS) {
        rte_panic("clEnqueueMapBuffer()@PhiComputeContext() failed to map the checkbit region!\n");
    }
    clev = nullptr;
}

PhiComputeContext::~PhiComputeContext()
{
    clReleaseCommandQueue(clqueue);
    delete dev_mempool_in;
    delete dev_mempool_out;
    delete cpu_mempool_in;
    delete cpu_mempool_out;
}

int PhiComputeContext::alloc_input_buffer(size_t size, void **host_ptr, memory_t *dev_mem)
{
    *host_ptr      = cpu_mempool_in->alloc(size);
    dev_mem->clmem = dev_mempool_in->alloc(size);
    return 0;
}

int PhiComputeContext::alloc_output_buffer(size_t size, void **host_ptr, memory_t *dev_mem)
{
    *host_ptr      = cpu_mempool_in->alloc(size);
    dev_mem->clmem = dev_mempool_in->alloc(size);
    return 0;
}

void PhiComputeContext::clear_io_buffers()
{
    cpu_mempool_in->reset();
    cpu_mempool_out->reset();
    dev_mempool_in->reset();
    dev_mempool_out->reset();
}

void *PhiComputeContext::get_host_input_buffer_base()
{
    return cpu_mempool_in->get_base_ptr();
}

memory_t PhiComputeContext::get_device_input_buffer_base()
{
    memory_t ret;
    ret.clmem = clbuf;
    return ret;
}

size_t PhiComputeContext::get_total_input_buffer_size()
{
    assert(cpu_mempool_in->get_alloc_size() == dev_mempool_in->get_alloc_size());
    return cpu_mempool_in->get_alloc_size();
}

void PhiComputeContext::set_io_buffers(void *in_h, memory_t in_d, size_t in_sz,
                       void *out_h, memory_t out_d, size_t out_sz)
{
    this->in_h = in_h;
    this->in_d = in_d;
    this->out_h = out_h;
    this->out_d = out_d;
    this->in_sz = in_sz;
    this->out_sz = out_sz;
}

void PhiComputeContext::set_io_buffer_elemsizes(size_t *in_h, memory_t in_d, size_t in_sz,
                                                size_t *out_h, memory_t out_d, size_t out_sz)
{
    this->in_elemsizes_h   = in_h;
    this->in_elemsizes_d   = in_d;
    this->out_elemsizes_h  = out_h;
    this->out_elemsizes_d  = out_d;
    this->in_elemsizes_sz  = in_sz;
    this->out_elemsizes_sz = out_sz;
}

int PhiComputeContext::enqueue_memwrite_op(void *host_buf, memory_t dev_buf, size_t offset, size_t size)
{
    return (int) clEnqueueWriteBuffer(clqueue, dev_buf.clmem, CL_FALSE, offset, size, host_buf, 0, NULL, &clev);
}

int PhiComputeContext::enqueue_memread_op(void *host_buf, memory_t dev_buf, size_t offset, size_t size)
{
    return (int) clEnqueueReadBuffer(clqueue, dev_buf.clmem, CL_FALSE, offset, size, host_buf, 0, NULL, &clev);
}

int PhiComputeContext::enqueue_kernel_launch(kernel_t kernel, struct resource_param *res,
                                             struct kernel_arg *args, size_t num_args)
{
    phiSafeCall(clSetKernelArg(kernel.clkernel, 0, sizeof(cl_mem), &in_d.clmem));
    phiSafeCall(clSetKernelArg(kernel.clkernel, 1, sizeof(cl_mem), &out_d.clmem));
    phiSafeCall(clSetKernelArg(kernel.clkernel, 2, sizeof(cl_mem), &in_elemsizes_d.clmem));
    phiSafeCall(clSetKernelArg(kernel.clkernel, 3, sizeof(cl_mem), &out_elemsizes_d.clmem));
    phiSafeCall(clSetKernelArg(kernel.clkernel, 4, sizeof(cl_uint), &res->num_workitems));
    phiSafeCall(clSetKernelArg(kernel.clkernel, 5, sizeof(cl_mem), &checkbits_d.clmem));
    for (unsigned i = 0; i < num_args; i++) {
        phiSafeCall(clSetKernelArg(kernel.clkernel, 6 + i, args[i].size, args[i].ptr));
    }
    clear_checkbits(res->num_workgroups);
    state = ComputeContext::RUNNING;
    phiSafeCall(clEnqueueNDRangeKernel(clqueue, kernel.clkernel, 1, NULL,
                                       &res->num_workitems, &res->num_threads_per_workgroup,
                                       0, NULL, &clev));
    return 0;
}

int PhiComputeContext::enqueue_event_callback(void (*func_ptr)(ComputeContext *ctx, void *user_arg), void *user_arg)
{
    auto cb = [](cl_event ev, cl_int status, void *user_data)
    {
        assert(status == CL_COMPLETE);
        struct phi_event_context *cectx = (struct phi_event_context *) user_data;
        cectx->callback(cectx->computectx, cectx->user_arg);
        delete cectx;
    };
    // TODO: how to avoid using new/delete?
    struct phi_event_context *cectx = new struct phi_event_context;
    cectx->computectx = this;
    cectx->callback = func_ptr;
    cectx->user_arg = user_arg;
    phiSafeCall(clEnqueueMarker(clqueue, &clev_marker));
    phiSafeCall(clSetEventCallback(clev_marker, CL_COMPLETE, cb, cectx));
    return 0;
}


// vim: ts=8 sts=4 sw=4 et