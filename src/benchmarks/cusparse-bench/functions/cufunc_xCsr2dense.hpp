/* ************************************************************************
 * Copyright 2015 Advanced Micro Devices, Inc.
 * ************************************************************************/

#ifndef CUBLAS_BENCHMARK_xCsr2dense_HXX__
#define CUBLAS_BENCHMARK_xCsr2dense_HXX__

#include "cufunc_common.hpp"
#include "include/io-exception.hpp"

template <typename T>
class xCsr2dense : public cusparseFunc
{
public:
    xCsr2dense( StatisticalTimer& timer ): cusparseFunc( timer )
    {
        cusparseStatus_t err = cusparseCreateMatDescr( &descrA );
        CUDA_V_THROW( err, "cusparseCreateMatDescr failed" );

        err = cusparseSetMatType( descrA, CUSPARSE_MATRIX_TYPE_GENERAL );
        CUDA_V_THROW( err, "cusparseSetMatType failed" );

        err = cusparseSetMatIndexBase( descrA, CUSPARSE_INDEX_BASE_ZERO );
        CUDA_V_THROW( err, "cusparseSetMatIndexBase failed" );
    }

    ~xCsr2dense( )
    {
        cusparseDestroyMatDescr( descrA );
    }

    void call_func( )
    {
        timer.Start( timer_id );
        xCsr2dense_Function( true );
        timer.Stop( timer_id );
    }

    double gflops( )
    {
        return 0.0;
    }

    std::string gflops_formula( )
    {
        return "N/A";
    }

    double bandwidth( )
    {
        //  Assuming that accesses to the vector always hit in the cache after the first access
        //  There are NNZ integers in the cols[ ] array
        //  You access each integer value in row_delimiters[ ] once.
        //  There are NNZ float_types in the vals[ ] array
        //  You read num_cols floats from the vector, afterwards they cache perfectly.
        //  Finally, you write num_rows floats out to DRAM at the end of the kernel.
        return ( sizeof( int )*( n_vals + n_rows ) + sizeof( T ) * ( n_vals + n_cols + n_rows ) ) / time_in_ns( );
    }

    std::string bandwidth_formula( )
    {
        return "GiB/s";
    }

    void setup_buffer( double alpha, double beta, const std::string& path )
    {
        initialize_scalars( alpha, beta );

        if (csrMatrixfromFile( row_offsets, col_indices, values, path.c_str( ) ) )
        {
            throw clsparse::io_exception( "Could not read matrix market header from disk" );
        }
        n_rows = row_offsets.size( );
        n_cols = col_indices.size( );
        n_vals = values.size( );

        cudaError_t err = cudaMalloc( (void**) &device_row_offsets, row_offsets.size( ) * sizeof( int ) );
        CUDA_V_THROW( err, "cudaMalloc device_row_offsets" );

        err = cudaMalloc( (void**) &device_col_indices, col_indices.size( ) * sizeof( int ) );
        CUDA_V_THROW( err, "cudaMalloc device_col_indices" );

        err = cudaMalloc( (void**) &device_values, values.size( ) * sizeof( T ) );
        CUDA_V_THROW( err, "cudaMalloc device_values" );

        err = cudaMalloc( (void**) &device_A, n_rows * n_cols * sizeof( T ) );
        CUDA_V_THROW( err, "cudaMalloc device_A" );
    }

    void initialize_cpu_buffer( )
    {
    }

    void initialize_gpu_buffer( )
    {
        cudaError_t err = cudaMemcpy( device_row_offsets, &row_offsets[ 0 ], row_offsets.size( ) * sizeof( int ), cudaMemcpyHostToDevice );
        CUDA_V_THROW( err, "cudaMalloc device_row_offsets" );

        err = cudaMemcpy( device_col_indices, &col_indices[ 0 ], col_indices.size( ) * sizeof( int ), cudaMemcpyHostToDevice );
        CUDA_V_THROW( err, "cudaMalloc device_col_indices" );

        err = cudaMemcpy( device_values, &values[ 0 ], values.size( ) * sizeof( T ), cudaMemcpyHostToDevice );
        CUDA_V_THROW( err, "cudaMalloc device_values" );

        err = cudaMemset( device_A, 0x0, n_rows * n_cols * sizeof( T ) );
        CUDA_V_THROW( err, "cudaMalloc device_A" );
    }

    void reset_gpu_write_buffer( )
    {
        cudaError_t err = cudaMemset( device_A, 0x0, n_rows * n_cols * sizeof( T ) );
        CUDA_V_THROW( err, "cudaMemset reset_gpu_write_buffer" );

    }

    void read_gpu_buffer( )
    {
    }

    void releaseGPUBuffer_deleteCPUBuffer( )
    {
        //this is necessary since we are running a iteration of tests and calculate the average time. (in client.cpp)
        //need to do this before we eventually hit the destructor
        CUDA_V_THROW( cudaFree( device_values  ), "cudafree device_values" );
        CUDA_V_THROW( cudaFree( device_row_offsets ), "cudafree device_row_offsets" );
        CUDA_V_THROW( cudaFree( device_col_indices ), "cudafree device_col_indices" );
        CUDA_V_THROW( cudaFree( device_A ), "cudafree device_A" );

        row_offsets.clear( );
        col_indices.clear( );
        values.clear( );
    }

protected:
    void initialize_scalars( double pAlpha, double pBeta )
    {
    }

private:
    void xCsr2dense_Function( bool flush );

    //host matrix definition
    std::vector< int > row_offsets;
    std::vector< int > col_indices;
    std::vector< T > values;
    int n_rows;
    int n_cols;
    int n_vals;

    cusparseMatDescr_t descrA;

    // device CUDA pointers
    int* device_row_offsets;
    int* device_col_indices;
    T* device_values;
    T* device_A;

}; // class xCsr2dense

template<>
void
xCsr2dense<float>::
xCsr2dense_Function( bool flush )
{
    cuSparseStatus =  cusparseScsr2dense( handle,
                                          n_rows,
                                          n_cols,
                                          descrA,
                                          device_values,
                                          device_row_offsets,
                                          device_col_indices,
                                          device_A,
                                          n_rows );
    CUDA_V_THROW( cuSparseStatus, "cusparseScsr2dense" );

    cudaDeviceSynchronize( );
}

//template<>
//void
//xCsr2dense<double>::
//xCsr2dense_Function( bool flush )
//{
//    cuSparseStatus = cusparseDcsr2dense( handle,
//                                         n_rows,
//                                         n_cols,
//                                         descrA,
//                                         device_values,
//                                         device_row_offsets,
//                                         device_col_indices,
//                                         device_A,
//                                         n_rows );
//    CUDA_V_THROW( cuSparseStatus, "cusparseDcsr2dense" );
//
//    cudaDeviceSynchronize( );
//}

#endif // ifndef CUBLAS_BENCHMARK_xCsr2dense_HXX__
