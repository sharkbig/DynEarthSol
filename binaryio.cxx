#include <cstdio>
#include <iostream>

#include "constants.hpp"
#include "parameters.hpp"
#include "binaryio.hpp"

/*****************************************************************************
 * The format of the binary file:
 * 1  The first 'headerlen' bytes are ASCII text.
 *   1.1  The 1st line in the header is the revision string. Starting with
 *        "# DynEarthSol ndims=%1 revision=%2", with %1 equal to 2 or 3
 *        (indicating 2D or 3D simulation) and %2 an integer.
 *   1.2  The following lines are 'name', 'position' pairs, separated by a
 *        TAB character. This line tells the name of the data and the
 *        starting position (in bytes) of the data in this file.
 * 2  The rests are binary data.
 ****************************************************************************/

namespace {
    const std::size_t headerlen = 4096;
    const char revision_str[] = "# DynEarthSol ndims="
#ifdef THREED
        "3"
#else
        "2"
#endif
        " revision=1\n";
}


/* Not using C++ stream IO for bulk file io since it can be much slower than C stdio. */

BinaryOutput::BinaryOutput(const char *filename)
{
    f = std::fopen(filename, "w");
    if (f == NULL) {
        std::cerr << "Error: cannot open file: " << filename << '\n';
        std::exit(1);
    }

    header = new char[headerlen]();
    hd_pos = std::strcat(header, revision_str);
    eof_pos = headerlen;

    std::fseek(f, eof_pos, SEEK_SET);
}


BinaryOutput::~BinaryOutput()
{
    /* write header buffer to the beginning of file */
    std::fseek(f, 0, SEEK_SET);
    std::fwrite(header, sizeof(char), headerlen, f);
    std::fclose(f);

    delete [] header;
}


void BinaryOutput::write_header(const char *name)
{
    /* write to header buffer */
    const std::size_t bsize = 256;
    char buffer[bsize];
    std::size_t len = std::snprintf(buffer, bsize, "%s\t%ld\n", name, eof_pos);
    if (len >= bsize) {
        std::cerr << "Error: exceeding buffer length at Output::write_array, name=" << name
                  << " eof_position=" << eof_pos << '\n';
        std::exit(1);
    }
    if (len >= headerlen - (hd_pos - header)*sizeof(char)) {
        std::cerr << "Error: exceeding header length at Output::write_array, name=" << name
                  << " eof_position=" << eof_pos << '\n';
        std::exit(1);
    }
    hd_pos = std::strncat(hd_pos, buffer, len);
}


template <typename T>
void BinaryOutput::write_array(const std::vector<T>& A, const char *name)
{
    write_header(name);
    std::size_t n = std::fwrite(A.data(), sizeof(T), A.size(), f);
    eof_pos += n * sizeof(T);
}


template <typename T, int N>
void BinaryOutput::write_array(const Array2D<T,N>& A, const char *name)
{
    write_header(name);
    std::size_t n = std::fwrite(A.data(), sizeof(T), A.num_elements(), f);
    eof_pos += n * sizeof(T);
}


// explicit instantiation
template
void BinaryOutput::write_array<int>(const std::vector<int>& A, const char *name);
template
void BinaryOutput::write_array<double>(const std::vector<double>& A, const char *name);

template
void BinaryOutput::write_array<double,NDIMS>(const Array2D<double,NDIMS>& A, const char *name);
template
void BinaryOutput::write_array<double,NSTR>(const Array2D<double,NSTR>& A, const char *name);
#ifdef THREED // when 2d, NSTR == NODES_PER_ELEM == 3
template
void BinaryOutput::write_array<double,NODES_PER_ELEM>(const Array2D<double,NODES_PER_ELEM>& A, const char *name);
#endif
template
void BinaryOutput::write_array<double,1>(const Array2D<double,1>& A, const char *name);
template
void BinaryOutput::write_array<int,NODES_PER_ELEM>(const Array2D<int,NODES_PER_ELEM>& A, const char *name);
template
void BinaryOutput::write_array<int,NDIMS>(const Array2D<int,NDIMS>& A, const char *name);
template
void BinaryOutput::write_array<int,1>(const Array2D<int,1>& A, const char *name);
