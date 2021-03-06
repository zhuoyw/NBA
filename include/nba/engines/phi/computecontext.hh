#ifndef __NBA_PHI_COMPUTECTX_HH__
#define __NBA_PHI_COMPUTECTX_HH__

#include <deque>
#include <CL/opencl.h>

#include <nba/framework/computedevice.hh>
#include <nba/framework/computecontext.hh>
#include <nba/engines/phi/utils.hh>
#include <nba/engines/phi/mempool.hh>

namespace nba
{

#define PHI_MAX_KERNEL_ARGS     (16)

class PhiComputeContext: public ComputeContext
{
friend class PhiComputeDevice;

private:
    PhiComputeContext(unsigned ctx_id, ComputeDevice *mother_device);

public:
    virtual ~PhiComputeContext();

    io_base_t alloc_io_base();
    int alloc_input_buffer(io_base_t io_base, size_t size, void **host_ptr, memory_t *dev_mem);
    int alloc_output_buffer(io_base_t io_base, size_t size, void **host_ptr, memory_t *dev_mem);
    void get_input_current_pos(io_base_t io_base, void **host_ptr, memory_t *dev_mem) const;
    void get_output_current_pos(io_base_t io_base, void **host_ptr, memory_t *dev_mem) const;
    size_t get_input_size(io_base_t io_base) const;
    size_t get_output_size(io_base_t io_base) const;
    void clear_io_buffers(io_base_t io_base);

    void clear_kernel_args() { }
    void push_kernel_arg(struct kernel_arg &arg) { }

    int enqueue_memwrite_op(void *host_buf, memory_t dev_buf, size_t offset, size_t size);
    int enqueue_memread_op(void *host_buf, memory_t dev_buf, size_t offset, size_t size);
    int enqueue_kernel_launch(kernel_t kernel, struct resource_param *res);
    int enqueue_event_callback(void (*func_ptr)(ComputeContext *ctx, void *user_arg), void *user_arg);

    void sync()
    {
        clFinish(clqueue);
    }

    bool query()
    {
        /* Check the "last" issued event.
         * Here is NOT a synchronization point. */
        cl_int status;
        if (clev == nullptr) // No async commands are issued.
            return true;
        phiSafeCall(clGetEventInfo(clev, CL_EVENT_COMMAND_EXECUTION_STATUS,
                                   sizeof(cl_int), &status, NULL));
        return (status == CL_COMPLETE);
    }

    uint8_t *get_device_checkbits()
    {
        assert(false, "not implemented");
        return nullptr;
    }

    uint8_t *get_host_checkbits()
    {
        return checkbits_h;
    }

    void clear_checkbits(unsigned num_workgroups)
    {
        unsigned n = (num_workgroups == 0) ? MAX_BLOCKS : num_workgroups;
        for (unsigned i = 0; i < num_workgroups; i++)
            checkbits_h[i] = 0;
    }

    static const int MAX_BLOCKS = 16384;

private:
    memory_t checkbits_d;
    uint8_t *checkbits_h;
    cl_command_queue clqueue;
    cl_event clev;
    cl_event clev_marker;
    CLMemoryPool *_mempool_in[NBA_MAX_IO_BASES];
    CLMemoryPool *_mempool_out[NBA_MAX_IO_BASES];

    size_t num_kernel_args;
    struct kernel_arg kernel_args[CUDA_MAX_KERNEL_ARGS];
};

}
#endif /* __NBA_PHI_COMPUTECTX_HH__ */

// vim: ts=8 sts=4 sw=4 et
