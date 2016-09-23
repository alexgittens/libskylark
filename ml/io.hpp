#ifndef IO_HPP_
#define IO_HPP_

#include <boost/mpi.hpp>
#include <sstream>
#include <cstdlib>
#include <string>
#include <El.hpp>
#include <El/core/types.h>
#include "../base/sparse_matrix.hpp"
#include "options.hpp"

namespace bmpi =  boost::mpi;

#ifdef SKYLARK_HAVE_HDF5
#include <H5Cpp.h>

int write_hdf5(const boost::mpi::communicator &comm,
    std::string fName, El::Matrix<double>& X,
    El::Matrix<double>& Y) {

    ElInt n, d;
    boost::mpi::reduce(comm, X.Width(), n, std::plus<ElInt>(), 0);
    d = X.Height();


    if (comm.rank() == 0) {
        try {
            std::cout << "Writing to file " << fName << " " << n << "x" << d << std::endl;

            H5::Exception::dontPrint();

            H5::H5File file( fName, H5F_ACC_TRUNC );
            H5::FloatType datatype( H5::PredType::NATIVE_DOUBLE );
            datatype.setOrder( H5T_ORDER_LE );

            hsize_t dimsf[2]; // dataset dimensions
            dimsf[0] = n;
            dimsf[1] = d;
            H5::DataSpace dataspaceX(2, dimsf);
            H5::DataSet datasetX = file.createDataSet("X", datatype, dataspaceX);

            dimsf[0] = n;
            H5::DataSpace dataspaceY(1, dimsf);
            H5::DataSet datasetY = file.createDataSet("Y", datatype, dataspaceY);

            hsize_t stride[2]; // Stride of hyperslab
            hsize_t block[2];  // Block sizes
            hsize_t mstart[2];
            stride[0] = 1; stride[1] = 1;
            block[0] = 1; block[1] = 1;
            mstart[0] = 0; mstart[0] = 0;

            int sumn = 0;
            for(int p = 0; p < comm.size(); p++) {
                El::Matrix<double> XX, YY;
                if (p == 0) {
                    XX = X;
                    YY = Y;
                } else {
                    int nn;
                    comm.recv(p, 1, nn);
                    XX.Resize(d, nn);
                    YY.Resize(nn, 1);
                    comm.recv(p, 2, XX.Buffer(), nn * d);
                    comm.recv(p, 3, YY.Buffer(), nn);

                }

                hsize_t start[2]; // Start of hyperslab
                hsize_t count[2];  // Block count

                start[0] = sumn; start[1] = 0;
                count[0] = XX.Width(); count[1] = d;
                dataspaceX.selectHyperslab(H5S_SELECT_SET, count, start, stride, block);
                H5::DataSpace memspaceX(2, count);
                memspaceX.selectHyperslab(H5S_SELECT_SET, count, mstart, stride, block);
                datasetX.write(XX.Buffer(), H5::PredType::NATIVE_DOUBLE,
                    memspaceX, dataspaceX);
                dataspaceX.selectNone();
                dataspaceY.selectHyperslab(H5S_SELECT_SET, count, start, stride, block);
                H5::DataSpace memspaceY(1, count);
                memspaceX.selectHyperslab(H5S_SELECT_SET, count, mstart, stride, block);
                datasetY.write(YY.Buffer(), H5::PredType::NATIVE_DOUBLE,
                    memspaceY, dataspaceY);
                dataspaceY.selectNone();
                file.flush(H5F_SCOPE_GLOBAL);
                sumn += XX.Width();
            }

            file.close();
        }
        // catch failure caused by the H5File operations
        catch( H5::FileIException error ) {
            error.printError();
            return -1;
        }
        // catch failure caused by the DataSet operations
        catch( H5::DataSetIException error ) {
            error.printError();
            return -1;
        }
        // catch failure caused by the DataSpace operations
        catch( H5::DataSpaceIException error ) {
            error.printError();
            return -1;
        }
        // catch failure caused by the DataSpace operations
        catch( H5::DataTypeIException error ) {
            error.printError();
            return -1;
        }
    } else {
        comm.send(0, 1, X.Width());
        comm.send(0, 2, X.Buffer(), X.Width() * X.Height());
        comm.send(0, 3, Y.Buffer(), Y.Height());
    }

    comm.barrier();

    return 0; // successfully terminated
}

int write_hdf5(std::string fName, skylark::base::sparse_matrix_t<double>& X,
    El::Matrix<double>& Y) {

    try {

        std::cout << "Writing to file " << fName << std::endl;

        int* dimensions = new int[3];
        dimensions[0] = X.height();
        dimensions[1] = X.width();
        dimensions[2] = X.nonzeros();

        H5::Exception::dontPrint();

        H5::H5File file( fName, H5F_ACC_TRUNC );

        // write dimensions
        {
            hsize_t dim[1]; // dataset dimensions
            dim[0] = 3;
            H5::DataSpace dataspace( 1, dim );
            H5::FloatType datatype( H5::PredType::NATIVE_INT );
            H5::DataSet dataset = file.createDataSet( "dimensions", datatype, dataspace );
            dataset.write(dimensions, H5::PredType::NATIVE_INT);
        }

        // write col_ptr
        {
            std::cout << "Writing col_ptr" << std::endl;
            hsize_t dim[1]; // dataset dimensions
            dim[0] = dimensions[1]+1;
            H5::DataSpace dataspace( 1, dim );
            H5::FloatType datatype( H5::PredType::NATIVE_INT );
            H5::DataSet dataset = file.createDataSet( "indptr", datatype, dataspace );
            dataset.write(X.indptr(), H5::PredType::NATIVE_INT);
        }

        // write row_ind
        {
            std::cout << "Writing row indices" << std::endl;
            hsize_t dim[1]; // dataset dimensions
            dim[0] = dimensions[2];
            H5::DataSpace dataspace( 1, dim );
            H5::FloatType datatype( H5::PredType::NATIVE_INT );
            H5::DataSet dataset = file.createDataSet( "indices", datatype, dataspace );
            dataset.write(X.indices(), H5::PredType::NATIVE_INT);
        }
        // write row_ind
        {
            std::cout << "Writing values" << std::endl;
            hsize_t dim[1]; // dataset dimensions
            dim[0] = dimensions[2];
            H5::DataSpace dataspace( 1, dim );
            H5::FloatType datatype( H5::PredType::NATIVE_DOUBLE );
            H5::DataSet dataset = file.createDataSet( "values", datatype, dataspace );
            dataset.write(X.values(), H5::PredType::NATIVE_DOUBLE);
        }


        // write values
        {
            std::cout << "Writing targets" << std::endl;
            hsize_t dim[1]; // dataset dimensions
            dim[0] = dimensions[1];
            H5::DataSpace dataspace( 1, dim );
            H5::FloatType datatype( H5::PredType::NATIVE_DOUBLE );
            H5::DataSet dataset = file.createDataSet( "Y", datatype, dataspace );
            dataset.write(Y.Buffer(), H5::PredType::NATIVE_DOUBLE);
        }

        delete[] dimensions;
        file.close();

    }
    // catch failure caused by the H5File operations
    catch( H5::FileIException error )
        {
            error.printError();
            return -1;
        }
    // catch failure caused by the DataSet operations
    catch( H5::DataSetIException error )
        {
            error.printError();
            return -1;
        }
    // catch failure caused by the DataSpace operations
    catch( H5::DataSpaceIException error )
        {
            error.printError();
            return -1;
        }
    // catch failure caused by the DataSpace operations
    catch( H5::DataTypeIException error )
        {
            error.printError();
            return -1;
        }
    return 0; // successfully terminated
}

void read_hdf5_dataset(H5::H5File& file, std::string name, int* buf, int offset, int count) {
    std::cout << "reading HDF5 dataset " << name << std::endl;
    H5::DataSet dataset = file.openDataSet(name);
    H5::DataSpace filespace = dataset.getSpace();
    hsize_t dims[1];
    dims[0] = count;
    H5::DataSpace mspace(1,dims);
    hsize_t count1[1];
    count1[0] = count;
    hsize_t offset1[1];
    offset1[0] = offset;
    filespace.selectHyperslab( H5S_SELECT_SET, count1, offset1 );
    dataset.read(buf, H5::PredType::NATIVE_INT, mspace, filespace);
}

void read_hdf5_dataset(H5::H5File& file, std::string name, double* buf, int offset, int count) {
    std::cout << "reading HDF5 dataset " << name << std::endl;
    H5::DataSet dataset = file.openDataSet(name);
    H5::DataSpace filespace = dataset.getSpace();
    hsize_t dims[1];
    dims[0] = count;
    H5::DataSpace mspace(1,dims);
    hsize_t count1[1];
    count1[0] = count;
    hsize_t offset1[1];
    offset1[0] = offset;
    filespace.selectHyperslab( H5S_SELECT_SET, count1, offset1 );
    dataset.read(buf, H5::PredType::NATIVE_DOUBLE, mspace, filespace);
}


void read_hdf5(const boost::mpi::communicator &comm, std::string fName,
    skylark::base::sparse_matrix_t<double>& X,
    El::Matrix<double>& Y, int min_d = 0) {

    try {
        int rank = comm.rank();
        int size = comm.size();

	bmpi::timer timer;
	if (rank==0)
            std::cout << "Reading sparse matrix from HDF5 file " << fName << std::endl;

	H5::H5File file( fName, H5F_ACC_RDONLY );

	int* dimensions = new int[3];

	read_hdf5_dataset(file, "dimensions", dimensions, 0, 3);

	int d = dimensions[0];
	int n = dimensions[1];
	int nnz = dimensions[3];

	if (min_d > 0)
            d = std::max(d, min_d);

	int* indptr = new int[n+1];
	read_hdf5_dataset(file, "indptr", indptr, 0, n+1);

	boost::mpi::broadcast(comm, n, 0);
	boost::mpi::broadcast(comm, d, 0);

        // Number of examples per process
	int* examples_allocation = new int[size];
	int chunksize = (int) n / size;
	int leftover = n % size;
	for(int i=0;i< size;i++)
            examples_allocation[i] = chunksize;
	for(int i=0;i<leftover;i++)
            examples_allocation[i]++;

	comm.barrier();

        // read chunks on rank = 0 and send
        if(rank==0) {
            int examples_local, nnz_start_index = 0, nnz_end_index = 0, nnz_local;
            int examples_total = 0;
            // loop and splice out chunk from the hdf5 file and then send to other processes
            for(int i=0;i<size;i++) {
                examples_local = examples_allocation[i];

                nnz_start_index = indptr[examples_total]; // start index of {examples_total+1}^th example
                nnz_end_index = indptr[examples_total + examples_local];
                nnz_local = nnz_end_index - nnz_start_index;

                double *_values = new double[nnz_local];
                int *_rowind = new int[nnz_local];
                int *_col_ptr = new int[examples_local+1];
                double *y = new double[examples_local];

                read_hdf5_dataset(file, "values", _values, nnz_start_index, nnz_local);
                read_hdf5_dataset(file, "indices", _rowind, nnz_start_index, nnz_local);
                read_hdf5_dataset(file, "Y", y, examples_total, examples_local);

                for(int k=0;k<examples_local; k++)
                    _col_ptr[k] = indptr[examples_total+k] - nnz_start_index;
                _col_ptr[examples_local] = nnz_local;

                //std::copy(indptr + examples_local_cumulative, indptr + examples_local_cumulative + examples_local, _col_ptr);
                examples_total += examples_local;

                if (i==0) {

                    X.attach(_col_ptr, _rowind, _values, nnz_local, d, examples_local, true);
                    El::Matrix<double> Y2(examples_local, 1, y, 0);
                    Y = Y2;
                    delete[] y;
                    std::cout << "rank=0: Read " << examples_local << " x " << d << " with " << nnz_local << " nonzeros" << std::endl;

                } else {

                    int process = i;
                    std::cout << "Sending to " << process << std::endl;
                    comm.send(process, 1, examples_local);
                    comm.send(process, 2, d);
                    comm.send(process, 3, nnz_local);
                    comm.send(process, 4, _col_ptr, examples_local+1);
                    comm.send(process, 5, _rowind, nnz_local);
                    comm.send(process, 6, _values, nnz_local);
                    comm.send(process, 7, y, examples_local);
                    delete[] _values;
                    delete[] _rowind;
                    delete[] _col_ptr;
                    delete[] y;
                }
            }
        } else {

            for(int r=1; r<size; r++) {
                if ((rank==r) && examples_allocation[r]>0) {
                    int nnz_local, t, d;
                    comm.recv(0, 1, t);
                    comm.recv(0, 2, d);
                    comm.recv(0, 3, nnz_local);

                    double* values = new double[nnz_local];
                    int* rowind = new int[nnz_local];
                    int* col_ptr = new int[t+1];

                    comm.recv(0, 4, col_ptr, t+1);
                    comm.recv(0, 5, rowind, nnz_local);
                    comm.recv(0, 6, values, nnz_local);

                    //attach currently creates copies so we can delete the rest
                    X.attach(col_ptr, rowind, values, nnz_local, d, t, true);
                    // delete[] col_ptr;
                    // delete[] rowind;
                    // delete[] values;

                    double* y = new double[t];
                    comm.recv(0, 7, y, t);
                    El::Matrix<double> Y2(t, 1, y, 0);
                    Y = Y2; // copy
                    delete[] y;
                    //Y.Resize(t,1);
                    //Y.Attach(t,1,y,0);

                    std::cout << "rank=" << r << ": Received and read " << t << " x " << d << " with " << nnz_local << " nonzeros" << std::endl;
                }
            }
        }

        double readtime = timer.elapsed();
        if (rank==0)
            std::cout << "Read Matrix with dimensions: " << n << " by " << d << " (" << readtime << "secs)" << std::endl;

        comm.barrier();

	delete[] dimensions;
	file.close();
    }
    // catch failure caused by the H5File operations
    catch( H5::FileIException error )
        {
            error.printError();
            //return -1;
        }
    // catch failure caused by the DataSet operations
    catch( H5::DataSetIException error )
        {
            error.printError();
            //return -1;
        }
    // catch failure caused by the DataSpace operations
    catch( H5::DataSpaceIException error )
        {
            error.printError();
            //return -1;
        }
    // catch failure caused by the DataSpace operations
    catch( H5::DataTypeIException error )
        {
            error.printError();
            //return -1;
        }
    //return 0; // successfully terminated

}

void read_hdf5(const boost::mpi::communicator &comm, std::string fName,
    El::Matrix<double>&  Xlocal, El::Matrix<double>& Ylocal, int blocksize = 10000) {

    int rank = comm.rank();
    bmpi::timer timer;
    if (rank==0)
        std::cout << "Reading Dense matrix from HDF5 file " << fName << std::endl;


    El::DistMatrix<double, El::STAR, El::VC> X;
    El::DistMatrix<double, El::VC, El::STAR> Y;

    H5::H5File file( fName, H5F_ACC_RDONLY );
    H5::DataSet datasetX = file.openDataSet( "X" );
    H5::DataSpace filespaceX = datasetX.getSpace();
    //int ndims = filespaceX.getSimpleExtentNdims();
    hsize_t dimsX[2]; // dataset dimensions
    int ndims = filespaceX.getSimpleExtentDims( dimsX );
    hsize_t n = dimsX[0];
    hsize_t d = dimsX[1];

    H5::DataSet datasetY = file.openDataSet( "Y" );
    H5::DataSpace filespaceY = datasetY.getSpace();
    hsize_t dimsY[1]; // dataset dimensions

    hsize_t countX[2];
    hsize_t countY[1];

    int numblocks = ((int) n/ (int) blocksize); // of size blocksize
    int leftover = n % blocksize;
    int block = blocksize;

    hsize_t offsetX[2], offsetY[1];

    //        if (min_d > 0)
    //                   d = std::max(d, min_d);

    X.Resize(d, n);
    Y.Resize(n,1);

    El::Zeros(Xlocal, X.LocalHeight(), X.LocalWidth());
    El::Zeros(Ylocal, Y.LocalHeight(), 1);

    X.Attach(d,n,El::DefaultGrid(), 0,0,Xlocal);
    Y.Attach(n,1,El::DefaultGrid(), 0,0,Ylocal);

    for(int i=0; i<numblocks+1; i++) {

        if (i==numblocks)
            block = leftover;
        if (block==0)
            break;

        El::DistMatrix<double, El::CIRC, El::CIRC> x(d, block), y(block, 1);
        x.SetRoot(0);
        y.SetRoot(0);
        El::Zero(x);


        offsetX[0] = i*blocksize;
        offsetX[1] = 0;
        countX[0] = block;
        countX[1] = d;
        offsetY[0] = i*blocksize;
        countY[0] = block;


        filespaceX.selectHyperslab( H5S_SELECT_SET, countX, offsetX );
        filespaceY.selectHyperslab( H5S_SELECT_SET, countY, offsetY );

        if(rank==0) {
            std::cout << "Reading and distributing chunk " << i*blocksize << " to " << i*blocksize + block - 1 << " ("<< block << " elements )" << std::endl;

            double *Xdata = x.Matrix().Buffer();
            double *Ydata = y.Matrix().Buffer();

            dimsX[0] = block;
            dimsX[1] = d;
            H5::DataSpace mspace1(2, dimsX);
            datasetX.read( Xdata, H5::PredType::NATIVE_DOUBLE, mspace1, filespaceX );

            dimsY[0] = block;
            H5::DataSpace mspace2(1,dimsY);
            datasetY.read( Ydata, H5::PredType::NATIVE_DOUBLE, mspace2, filespaceY );

        }

        El::DistMatrix<double, El::STAR, El::VC> viewX;
        El::DistMatrix<double, El::VC, El::STAR> viewY;

        El::View(viewX, X, 0, i*blocksize, x.Height(), x.Width());
        El::View(viewY, Y, i*blocksize, 0, x.Width(), 1);

        viewX = x;
        viewY = y;

    }

    double readtime = timer.elapsed();
    if (rank==0)
        std::cout << "Read Matrix with dimensions: " << n << " by " << d << " (" << readtime << "secs)" << std::endl;
}
#endif

template<typename T>
void read_libsvm(const boost::mpi::communicator &comm, std::string fName,
    El::Matrix<T>& Xlocal, El::Matrix<T>& Ylocal,
    int min_d = 0, int blocksize = 10000) {

    MPI_Comm mpi_comm(comm);
    El::Grid grid(mpi_comm);
    El::DistMatrix<T, El::STAR, El::VC> X(grid);
    El::DistMatrix<T, El::VC, El::STAR> Y(grid);

    int rank = comm.rank();
    if (rank==0)
        std::cout << "Reading from file " << fName << std::endl;

    std::ifstream file(fName.c_str());
    std::string line;
    std::string token, val, ind;
    float label;
    unsigned int start = 0;
    unsigned int delim, t;
    int n = 0;
    int d = 0;
    int i, j, last;
    char c;

    bmpi::timer timer;


    // make one pass over the data to figure out dimensions - will pay in terms of preallocated storage.
    if (rank==0) {
        while(!file.eof()) {
            getline(file, line);
            if(line.length()==0) {
                break;
            }
            n++;
            delim = line.find_last_of(":");
            if(delim > line.length()) {
                continue;
            }
            t = delim;
            while(line[t]!=' ') {
                t--;
            }
            val = line.substr(t+1, delim - t);
            last = atoi(val.c_str());
            if (last>d)
                d = last;
        }
        if (min_d > 0)
            d = std::max(d, min_d);

        // prepare for second pass
        file.clear();
        file.seekg(0, std::ios::beg);
    }

    boost::mpi::broadcast(comm, n, 0);
    boost::mpi::broadcast(comm, d, 0);

    int numblocks = ((int) n/ (int) blocksize); // of size blocksize
    int leftover = n % blocksize;
    int block = blocksize;

    X.Resize(d, n);
    Y.Resize(n,1);

    El::Zeros(Xlocal, X.LocalHeight(), X.LocalWidth());
    El::Zeros(Ylocal, Y.LocalHeight(), 1);

    X.Attach(d,n,El::DefaultGrid(),0,0,Xlocal);
    Y.Attach(n,1,El::DefaultGrid(),0,0,Ylocal);


    for(int i=0; i<numblocks+1; i++) {

        if (i==numblocks)
            block = leftover;
        if (block==0)
            break;

        El::DistMatrix<double, El::CIRC, El::CIRC> x(d, block), y(block, 1);
        x.SetRoot(0);
        y.SetRoot(0);
        El::Zero(x);

        if(rank==0) {

            std::cout << "Reading and distributing chunk " << i*blocksize << " to " << i*blocksize + block - 1 << " ("<< block << " elements )" << std::endl;
            T *Xdata = x.Matrix().Buffer();
            T *Ydata = y.Matrix().Buffer();

            t = 0;
            while(!file.eof() && t<block) {
                getline(file, line);
                if( line.length()==0) {
                    break;
                }

                std::istringstream tokenstream (line);
                tokenstream >> label;
                Ydata[t] = label;

                while (tokenstream >> token)
                    {
                        delim  = token.find(':');
                        ind = token.substr(0, delim);
                        val = token.substr(delim+1); //.substr(delim+1);
                        j = atoi(ind.c_str()) - 1;
                        Xdata[t * d + j] = atof(val.c_str());
                    }

                t++;
            }
        }

        // The calls below should distribute the data to all the nodes.
        // if (rank==0)
        //    std::cout << "Distributing Data.." << std::endl;

        El::DistMatrix<T, El::STAR, El::VC> viewX;
        El::DistMatrix<T, El::VC, El::STAR> viewY;

        El::View(viewX, X, 0, i*blocksize, x.Height(), x.Width());
        El::View(viewY, Y, i*blocksize, 0, x.Width(), 1);

        viewX = x;
        viewY = y;

        //	X = x;
        //	Y = y;
    }

    double readtime = timer.elapsed();
    if (rank==0) {
        std::cout << "Read Matrix with dimensions: " << n << " by " << d << " (" << readtime << "secs)" << std::endl;
        // El::Print(X,"X",std::cout);
    }
}

template<typename T>
void read_libsvm(const boost::mpi::communicator &comm, std::string fName,
    skylark::base::sparse_matrix_t<T>& X, El::Matrix<T>& Y, int min_d = 0) {

    int rank = comm.rank();
    int size = comm.size();

    if (rank==0)
        std::cout << "Reading sparse matrix from file " << fName << std::endl;

    std::ifstream file(fName.c_str());
    std::string line;
    std::string token, val, ind;
    float label;
    unsigned int start = 0;
    unsigned int delim, t;
    int n = 0;
    int d = 0;
    int i, j, last;
    char c;
    int nnz=0;
    int nz;

    bmpi::timer timer;

    // make one pass over the data to figure out dimensions - will pay in terms of preallocated storage.
    if (rank==0) {
        while(!file.eof()) {
            getline(file, line);
            if(line.length()==0)
                break;
            delim = line.find_last_of(":");
            if(delim > line.length())
                continue;
            n++;
            t = delim;
            while(line[t]!=' ') {
                t--;
            }
            val = line.substr(t+1, delim - t);
            last = atoi(val.c_str());
            if (last>d)
                d = last;
        }
        if (min_d > 0)
            d = std::max(d, min_d);

        // prepare for second pass
        file.clear();
        file.seekg(0, std::ios::beg);
    }


    boost::mpi::broadcast(comm, n, 0);
    boost::mpi::broadcast(comm, d, 0);

    // Number of examples per process
    int* examples_allocation = new int[size];
    int chunksize = (int) n / size;
    int leftover = n % size;
    for(int i=0;i<size;i++)
        examples_allocation[i] = chunksize;
    for(int i=0;i<leftover;i++)
        examples_allocation[i]++;

    comm.barrier();

    // read chunks on rank = 0 and send
    if(rank==0) {

        std::vector<int> col_ptr;
        std::vector<int> rowind;
        std::vector<double> values;
        std::vector<double> y;

        int process = 0;
        int nnz_local = 0;
        int examples_local = 0;

        while(!file.eof()) {
            getline(file, line);
            examples_local++;
            if( line.length()==0) {
                break;
            }

            std::istringstream tokenstream (line);
            tokenstream >> label;
            y.push_back(label);
            col_ptr.push_back(nnz_local);

            while (tokenstream >> token)
                {
                    delim  = token.find(':');
                    ind = token.substr(0, delim);
                    val = token.substr(delim+1); //.substr(delim+1);
                    j = atoi(ind.c_str()) - 1;
                    rowind.push_back(j);
                    values.push_back(atof(val.c_str()));
                    nnz_local++;

                }

            if (examples_local == examples_allocation[process]) {
                if (process>0) { //send data from rank 0
                    std::cout << "Sending to " << process << std::endl;
                    col_ptr.push_back(nnz_local);
                    comm.send(process, 1, examples_local);
                    comm.send(process, 2, d);
                    comm.send(process, 3, nnz_local);
                    comm.send(process, 4, &col_ptr[0], col_ptr.size());
                    comm.send(process, 5, &rowind[0], rowind.size());
                    comm.send(process, 6, &values[0], values.size());
                    comm.send(process, 7, &y[0], y.size());
                }
                else { // rank == 0 - just create the sparse matrix

                    col_ptr.push_back(nnz_local); //last entry of col_ptr should be total number of nonzeros

                    // this is making a copy
                    double *_values = new double[values.size()];
                    int *_rowind = new int[rowind.size()];
                    int *_col_ptr = new int[col_ptr.size()];


                    // DO THE ACTUAL COPY!
                    std::copy(values.begin(), values.end(), _values);
                    std::copy(rowind.begin(), rowind.end(), _rowind);
                    std::copy(col_ptr.begin(), col_ptr.end(), _col_ptr);

                    X.attach(_col_ptr, _rowind, _values, nnz_local, d, examples_local, true);

                    El::Matrix<T> Y2(examples_local, 1, &y[0], 0);

                    //  Y.Resize(examples_local,1);
                    // but this is not!
                    //Y.Attach(examples_local,1,&y[0],0);
                    Y = Y2; // copy


                    std::cout << "rank=0: Read " << examples_local << " x " << d << " with " << nnz_local << " nonzeros" << std::endl;
                }
                process++;
                nnz_local = 0;
                examples_local = 0;
                col_ptr.clear();
                rowind.clear();
                values.clear();
                y.clear();
            }
        }


    } else {

        for(int r=1; r<size; r++) {
            if ((rank==r) && examples_allocation[r]>0) {
                int nnz_local, t, d;
                comm.recv(0, 1, t);
                comm.recv(0, 2, d);
                comm.recv(0, 3, nnz_local);

                double* values = new double[nnz_local];
                int* rowind = new int[nnz_local];
                int* col_ptr = new int[t+1];

                comm.recv(0, 4, col_ptr, t+1);
                comm.recv(0, 5, rowind, nnz_local);
                comm.recv(0, 6, values, nnz_local);

                //attach currently creates copies so we can delete the rest
                X.attach(col_ptr, rowind, values, nnz_local, d, t, true);
                // delete[] col_ptr;
                // delete[] rowind;
                // delete[] values;

                // TODO why not resize Y, and then just recive into buffer?
                double* y = new double[t];
                comm.recv(0, 7, y, t);
                El::Matrix<T> Y2(t, 1, y, 0);
                Y = Y2; // copy
                delete[] y;

                //Y.Resize(t,1);
                //Y.Attach(t,1,y,0);

                std::cout << "rank=" << r << ": Received and read " << t << " x " << d << " with " << nnz_local << " nonzeros" << std::endl;
            }
        }
    }
    //}

    delete[] examples_allocation;

    double readtime = timer.elapsed();
    if (rank==0)
        std::cout << "Read Matrix with dimensions: " << n << " by " << d << " (" << readtime << "secs)" << std::endl;

    comm.barrier();
}


template <class InputType, class LabelType>
void read(const boost::mpi::communicator &comm,
    int fileformat, std::string filename, InputType& X, LabelType& Y, int d=0) {

    switch(fileformat) {
    case LIBSVM_DENSE: case LIBSVM_SPARSE:
        {
            read_libsvm(comm, filename, X, Y, d);
            break;
        }
    case HDF5_DENSE: case HDF5_SPARSE:
        {
#ifdef SKYLARK_HAVE_HDF5
            read_hdf5(comm, filename, X, Y);
#else
            // TODO
#endif
            break;
        }
    }

}

void read_model_file(std::string fName, El::Matrix<double>& W) {
    std::ifstream file(fName.c_str());
    std::string line, token;
    std::string prefix = "# Dimensions";
    int i=0;
    int j;
    int m, n;
    while(!file.eof()) {
        getline(file, line);
        if(line.compare(0, prefix.size(), prefix) == 0) {
            std::istringstream tokenstream (line.substr(prefix.size(), line.size()));
            tokenstream >> token;

            m = atoi(token.c_str());
            tokenstream >> token;

            n = atoi(token.c_str());
            std::cout << "Read coefficients of size " << m << " x " << n << std::endl;
            W.Resize(m,n);
            continue;
        }
        else {
            if(line[0] == '#' || line.length()==0)
                continue;
        }

        std::istringstream tokenstream (line);
        j = 0;
        while (tokenstream >> token){
            W.Set(i,j, atof(token.c_str()));
            j++;
        }
        i++;
    }
}


std::string read_header(const boost::mpi::communicator &comm, std::string fName) {
    std::string line;
    if (comm.rank()==0) {
        std::ifstream file(fName.c_str());
        getline(file, line);
    }

    boost::mpi::broadcast(comm, line, 0);
    return line;
}


#endif /* IO_HPP_ */
