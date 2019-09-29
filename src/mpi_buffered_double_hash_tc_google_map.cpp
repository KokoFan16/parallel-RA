//#include <chrono>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <mpi.h>
#include "compat.h"
#include "tuple.h"
#include <unordered_set>


#define COL_COUNT 2

#include "btree/btree_map.h"

typedef btree::btree_map<u64, u64> Relation0Map;
typedef btree::btree_map<u64, btree::btree_map<u64, u64>* > Relation1Map;

static int rank = 0;
static u32 nprocs = 1;
static MPI_Comm comm;
static u32 global_row_count;



inline u64 tunedhash(const u8* bp, const u32 len)
{
    u64 h0 = 0xb97a19cb491c291d;
    u64 h1 = 0xc18292e6c9371a17;
    const u8* const ep = bp+len;
    while (bp < ep)
    {
        h1 ^= *bp;
        h1 *= 31;
        h0 ^= (((u64)*bp) << 17) ^ *bp;
        h0 *= 0x100000001b3;
        h0 = (h0 >> 7) | (h0 << 57);
        ++bp;
    }

    return h0 ^ h1;
}



u64 outer_hash(const u64 val)
{
    return tunedhash((u8*)(&val),sizeof(u64));
}


void parallel_read_input_relation_from_file_to_local_buffer(const char *file_name, u64** read_buffer, u32* local_row_count)
{
    if (rank == 0)
    {
        int global_col_count = 0;
        char meta_data_filename[1024];
        sprintf(meta_data_filename, "%s/meta_data.txt", file_name);
        printf("Opening File %s\n", meta_data_filename);

        FILE *fp_in;
        fp_in = fopen(meta_data_filename, "r");
        if (fscanf (fp_in, "(row count)\n%d\n(col count)\n%d", &global_row_count, &global_col_count) != 2)
        {
            printf("Wrong input format (Meta Data)\n");
            MPI_Abort(MPI_COMM_WORLD, -1);
        }
        assert(global_col_count == 2);
        fclose(fp_in);
    }

    MPI_Bcast(&global_row_count, 1, MPI_INT, 0, MPI_COMM_WORLD);


    /*
    int read_offset;
    read_offset = ceil((float)global_row_count / nprocs) * rank;
    if (read_offset + ceil((float)global_row_count / nprocs) > global_row_count)
        *local_row_count = global_row_count - read_offset;
    else
        *local_row_count = (int) ceil((float)global_row_count / nprocs);

    if (*local_row_count < 0)
        *local_row_count = 0;
    */

    u32 read_offset;
    read_offset = ceil((float)global_row_count / nprocs) * rank;
    if (read_offset > global_row_count)
    {
        *local_row_count = 0;
        return;
    }
    if (read_offset + ceil((float)global_row_count / nprocs) > global_row_count)
        *local_row_count = global_row_count - read_offset;
    else
        *local_row_count = (u32) ceil((float)global_row_count / nprocs);
    if (*local_row_count == 0)
        return;

    char data_filename[1024];
    sprintf(data_filename, "%s/data.raw", file_name);
    int fp = open(data_filename, O_RDONLY);

    //std::cout << "Global row count " << global_row_count << " Col count " << COL_COUNT << std::endl;
    //std::cout << "Local row count " << *local_row_count << std::endl;

    *read_buffer = new u64[*local_row_count * COL_COUNT];
    u64 rb_size = pread(fp, *read_buffer, *local_row_count * COL_COUNT * sizeof(u64), read_offset * COL_COUNT * sizeof(u64));
    if (rb_size != *local_row_count * COL_COUNT * sizeof(u64))
    {
        printf("Wrong input format (Meta Data)\n");
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    close(fp);

    //if (rank == 0)
    //printf("Rank %d reads %d elements from %d offset from %s\n", rank, *local_row_count, read_offset, data_filename);

    return;
}

void buffer_data_to_hash_buffer(u32 local_number_of_rows, u64* input_data,  int hash_column_index, u64** outer_hash_data, u32* outer_hash_buffer_size, MPI_Comm comm, int buckets, u32* subbuckets_G, u32* subbuckets_T, int** gmap_sub_bucket_rank)
{
    /* process_size[j] stores the number of samples to be sent to process with rank j */
    int process_size[nprocs];
    memset(process_size, 0, nprocs * sizeof(int));

    /* vector[i] contains the data that needs to be sent to process i */
    std::vector<u64> process_data_vector[nprocs];

    if (hash_column_index == 0)
    {
        for (u32 i = 0; i < local_number_of_rows * COL_COUNT; i=i+2)
        {
            uint64_t bucket_id = outer_hash(input_data[i]) % buckets;
            uint64_t sub_bucket_id = outer_hash(input_data[i+1]) % subbuckets_G[bucket_id];

            uint64_t index = (outer_hash((bucket_id << 32)^sub_bucket_id))%nprocs;
            //int index = gmap_sub_bucket_rank[bucket_id][sub_bucket_id];
            process_size[index] = process_size[index] + COL_COUNT;

            process_data_vector[index].push_back(input_data[i]);
            process_data_vector[index].push_back(input_data[i + 1]);
        }
    }
    else
    {
        for (u32 i = 0; i < local_number_of_rows * COL_COUNT; i=i+2)
        {
            uint64_t bucket_id = outer_hash(input_data[i+1]) % buckets;
            uint64_t sub_bucket_id = outer_hash(input_data[i]) % subbuckets_T[bucket_id];

            uint64_t index = (outer_hash((bucket_id << 32)^sub_bucket_id))%nprocs;
            //int index = gmap_sub_bucket_rank[bucket_id][sub_bucket_id];
            process_size[index] = process_size[index] + COL_COUNT;

            process_data_vector[index].push_back(input_data[i]);
            process_data_vector[index].push_back(input_data[i + 1]);
        }
    }

    int prefix_sum_process_size[nprocs];
    memset(prefix_sum_process_size, 0, nprocs * sizeof(int));

    for (u32 i = 1; i < nprocs; i++)
        prefix_sum_process_size[i] = prefix_sum_process_size[i - 1] + process_size[i - 1];


    int process_data_buffer_size = prefix_sum_process_size[nprocs - 1] + process_size[nprocs - 1];
    u64* process_data = new u64[process_data_buffer_size];

    for (u32 i = 0; i < nprocs; i++)
        memcpy(process_data + prefix_sum_process_size[i], &process_data_vector[i][0], process_data_vector[i].size() * sizeof(u64));

    /* This step prepares for actual data transfer */
    /* Every process sends to every other process the amount of data it is going to send */
    int recv_process_size_buffer[nprocs];
    memset(recv_process_size_buffer, 0, nprocs * sizeof(int));
    MPI_Alltoall(process_size, 1, MPI_INT, recv_process_size_buffer, 1, MPI_INT, comm);

    int prefix_sum_recv_process_size_buffer[nprocs];
    memset(prefix_sum_recv_process_size_buffer, 0, nprocs * sizeof(int));
    for (u32 i = 1; i < nprocs; i++)
        prefix_sum_recv_process_size_buffer[i] = prefix_sum_recv_process_size_buffer[i - 1] + recv_process_size_buffer[i - 1];


    /* Sending data to all processes */
    /* What is the buffer size to allocate */
    *outer_hash_buffer_size = 0;
    for(u32 i = 0; i < nprocs; i++)
        *outer_hash_buffer_size = *outer_hash_buffer_size + recv_process_size_buffer[i];

#if 1
    uint total_row_size = 0;
    MPI_Allreduce(outer_hash_buffer_size, &total_row_size, 1, MPI_INT, MPI_SUM, comm);
    if(total_row_size != global_row_count * COL_COUNT)
    {
        printf("Incorrect distribution %d != %d %d\n", total_row_size, global_row_count, COL_COUNT);
        MPI_Abort(comm, -1);
    }
#endif

    *outer_hash_data = new u64[*outer_hash_buffer_size];
    MPI_Alltoallv(process_data, process_size, prefix_sum_process_size, MPI_UNSIGNED_LONG_LONG, *outer_hash_data, recv_process_size_buffer, prefix_sum_recv_process_size_buffer, MPI_UNSIGNED_LONG_LONG, comm);

    *outer_hash_buffer_size = *outer_hash_buffer_size / COL_COUNT;

    delete[] process_data;

    return;
}


Relation1Map *** parallel_map_join(Relation1Map*** delT, u32* dtmap, Relation1Map**& G, u32* gmap_bucket, Relation1Map**& T, u32* tmap, int lc, int* lb, int* running_t_count, double* running_time, u32 buckets, u32* subbuckets_G, u32* subbuckets_T, int** gmap_sub_bucket_rank, int** tmap_sub_bucket_rank, u32* g_t_bucket_indices, u32 g_t_actual_bucket_count, u32* g_bucket_indices, u32 g_actual_bucket_count, u32* g_recv_count, u32** g_recv_rank, int comm_type, int* gmap_distinct_sub_bucket_rank_count,  int** gmap_distinct_sub_bucket_rank, int* tmap_distinct_sub_bucket_rank_count, int** tmap_distinct_sub_bucket_rank)
{
    double comm_create_start = MPI_Wtime();
    double comm_create_end = MPI_Wtime();
    double join_time = 0;
    double outer_comm_time = 0;

    outer_comm_time = join_time + comm_create_start + comm_create_end;
    outer_comm_time = outer_comm_time /outer_comm_time ;

    tuple<2> dt;
    std::vector<tuple<2>> *process_data_vector;
    process_data_vector = new std::vector<tuple<2>>[nprocs];
    u64 tuple_count = 0;

    Relation1Map ***delTT = new Relation1Map**[buckets];
    for (u32 i = 0; i < buckets; i++)
    {
        delTT[i] = new Relation1Map*[subbuckets_T[i]];
        for (u32 j = 0; j < subbuckets_T[i]; j++)
            delTT[i][j] = new Relation1Map;
    }



    u64 **recvbuf = new u64*[buckets];
    memset(recvbuf, 0, sizeof(u64*) * buckets);
    u64 *total_buffer_size = new u64[buckets];
    memset(total_buffer_size, 0, sizeof(u64)*buckets);

    if (comm_type == 1)
    {
        comm_create_start = MPI_Wtime();

        // Send Join output
        // process_size[j] stores the number of samples to be sent to process with rank j
        int process_size[nprocs];
        memset(process_size, 0, nprocs * sizeof(int));

        std::vector<tuple<2>> delT_temp[buckets];
        int color[buckets];
        memset(color, 0, buckets * sizeof(int));

        MPI_Comm group_comm[buckets];

        for (u32 i = 0; i < buckets; i++)
        {
            if (dtmap[i] == 1 || gmap_bucket[i] == 1)
                color[i] = 1;

            for (u32 j = 0; j < subbuckets_T[i]; j++)
            {
                for (auto it = delT[i][j]->begin(); it != delT[i][j]->end(); it++)
                {
                    Relation0Map* it2 = it->second;
                    for (auto dit2 = it2->begin(); dit2 != it2->end(); dit2++)
                    {
                        dt[0] = it->first;
                        dt[1] = dit2->first;
                        delT_temp[i].push_back(dt);
                    }
                }
            }
            MPI_Comm_split(MPI_COMM_WORLD, color[i], rank, &(group_comm[i]));
        }
        comm_create_end = MPI_Wtime();


        int gnprocs = 0;
        double t1_s = MPI_Wtime();
        for (u32 i = 0; i < buckets; i++)
        {
            MPI_Comm_size(group_comm[i], &gnprocs);
            if (color[i] == 1)
            {
                int recvcounts[gnprocs];
                int displs[gnprocs];
                for (int n = 0; n < gnprocs; n++)
                {
                    recvcounts[n] = 1;
                    displs[n] = n;
                }

                /* This step prepares for actual data transfer */
                /* Every process sends to every other process the amount of data it is going to send */
                int recv_process_size_buffer_local[gnprocs];
                memset(recv_process_size_buffer_local, 0, gnprocs * sizeof(int));

                int buffer_size = delT_temp[i].size() * COL_COUNT;
                MPI_Allgatherv(&buffer_size, 1, MPI_INT, recv_process_size_buffer_local, recvcounts, displs, MPI_INT, group_comm[i]);

                int recv_process_prefix[gnprocs];
                memset(recv_process_prefix, 0, gnprocs * sizeof(int));

                for (int n = 0; n < gnprocs; n++)
                {
                    recv_process_prefix[n] = total_buffer_size[i];
                    total_buffer_size[i] = total_buffer_size[i] + recv_process_size_buffer_local[n];
                }

                recvbuf[i] = new u64[total_buffer_size[i]];

                MPI_Allgatherv(&delT_temp[i][0], buffer_size, MPI_UNSIGNED_LONG_LONG, recvbuf[i], recv_process_size_buffer_local, recv_process_prefix, MPI_UNSIGNED_LONG_LONG, group_comm[i]);
            }
            MPI_Comm_free(&group_comm[i]);
        }

        double t1_e = MPI_Wtime();
        outer_comm_time = (t1_e - t1_s);
    }
    else if (comm_type == 2)
    {
        double t1_s = MPI_Wtime();

        u32 *global_dtmap = new u32[buckets * nprocs];
        memset(global_dtmap, 0, sizeof(u32) * buckets * nprocs);
        MPI_Allgather(dtmap, buckets, MPI_INT, global_dtmap, buckets, MPI_INT, MPI_COMM_WORLD);
#if 1

        u32* dt_send_count = new u32[buckets];
        memset(dt_send_count, 0, buckets * sizeof(u32));

        for (u32 b = 0; b < buckets; b++)
        {
            for (u32 h = b; h < buckets * nprocs; h=h+buckets)
            {
                if (global_dtmap[h] == 1)
                    dt_send_count[b]++;
            }
            //std::cout << "dt_send_count[b]" << dt_send_count[b] << std::endl;
        }

        u32** dt_send_rank = new u32*[buckets];
        for (u32 b = 0; b < buckets; b++)
        {
            dt_send_rank[b] = new u32[dt_send_count[b]];
            memset(dt_send_rank[b], 0, dt_send_count[b] * sizeof(u32));
        }
        for (u32 b = 0; b < buckets; b++)
        {
            int rcount = 0;
            for (u32 h = b; h < buckets * nprocs; h=h+buckets)
            {
                if (global_dtmap[h] == 1)
                {
                    dt_send_rank[b][rcount] = h / buckets;
                    rcount++;
                }
            }
        }
#if 0
        for (u32 b = 0; b < buckets; b++)
            for (u32 k = 0; k < dt_send_count[b]; k++)
                std::cout << b << "  " << k << " " << dt_send_rank[b][k] << std::endl;
#endif

        delete[] global_dtmap;


        std::vector<tuple<2>> delT_temp[buckets];
        for (u32 i = 0; i < buckets; i++)
        {
            if (dtmap[i] == 1 || gmap_bucket[i] == 1)
            {
                for (u32 j = 0; j < subbuckets_T[i]; j++)
                {
                    for (auto it = delT[i][j]->begin(); it != delT[i][j]->end(); it++)
                    {
                        Relation0Map* it2 = it->second;
                        for (auto dit2 = it2->begin(); dit2 != it2->end(); dit2++)
                        {
                            dt[0] = it->first;
                            dt[1] = dit2->first;
                            delT_temp[i].push_back(dt);
                        }
                    }
                }

                int buffer_size = delT_temp[i].size() * COL_COUNT;
                u32* size_buf = new u32[dt_send_count[i]];
                memset(size_buf, 0, sizeof(u32) * dt_send_count[i]);
                u32 req_counter1 = 0;
                MPI_Request *req1 = new MPI_Request[g_recv_count[i] + dt_send_count[i]];
                MPI_Status *stat1 = new MPI_Status[g_recv_count[i] + dt_send_count[i]];

                // meta-data
                if (dtmap[i] == 1)
                {
                    for (u32 r = 0; r < g_recv_count[i]; r++)
                    {
                        MPI_Isend(&buffer_size, 1, MPI_INT, g_recv_rank[i][r], 123, MPI_COMM_WORLD, &req1[req_counter1]);
                        req_counter1++;
                    }
                }
                if (gmap_bucket[i] == 1)
                {
                    for (u32 r = 0; r < dt_send_count[i]; r++)
                    {
                        MPI_Irecv(size_buf + r, 1, MPI_INT, dt_send_rank[i][r], 123, MPI_COMM_WORLD, &req1[req_counter1]);
                        req_counter1++;
                    }
                }
                MPI_Waitall(req_counter1, req1, stat1);

                for (u32 r = 0; r < dt_send_count[i]; r++)
                    total_buffer_size[i] = total_buffer_size[i] + size_buf[r];


#if 1
                u32 req_counter2 = 0;
                MPI_Request *req2 = new MPI_Request[g_recv_count[i] + dt_send_count[i]];
                MPI_Status *stat2 = new MPI_Status[g_recv_count[i] + dt_send_count[i]];


                // data
                if (dtmap[i] == 1)
                {
                    for (u32 r = 0; r < g_recv_count[i]; r++)
                    {
                        if (delT_temp[i].size() != 0)
                        {
                            MPI_Isend(&delT_temp[i][0], delT_temp[i].size() * COL_COUNT, MPI_UNSIGNED_LONG_LONG, g_recv_rank[i][r], 123, MPI_COMM_WORLD, &req2[req_counter2]);
                            req_counter2++;
                        }
                    }
                }
                if (gmap_bucket[i] == 1)
                {
                    u32 offset = 0;

                    recvbuf[i] = new u64[total_buffer_size[i]];
                    for (u32 r = 0; r < dt_send_count[i]; r++)
                    {
                        if (size_buf[r] != 0)
                        {
                            MPI_Irecv(recvbuf[i] + offset, size_buf[r], MPI_UNSIGNED_LONG_LONG, dt_send_rank[i][r], 123, MPI_COMM_WORLD, &req2[req_counter2]);
                            offset = offset + size_buf[r];
                            req_counter2++;
                        }
                    }
                }
                MPI_Waitall(req_counter2, req2, stat2);
                delete[] req1;
                delete[] req2;

                delete[] stat1;
                delete[] stat2;
                delete[] size_buf;
#endif
            }
        }
#endif

        delete[] dt_send_count;
        for (u32 b = 0; b < buckets; b++)
            delete[] dt_send_rank[b];
        delete[] dt_send_rank;

        double t1_e = MPI_Wtime();
        outer_comm_time = (t1_e - t1_s);
    }
    else
    {
        double t1_s = MPI_Wtime();

        std::vector<tuple<2>> delT_temp[buckets];
        //for (u32 i = 0; i < buckets; i++)
#if 1
        for (u32 i3 = 0; i3 < g_t_actual_bucket_count; i3++)
        {
            u32 i = g_t_bucket_indices[i3];

            if (tmap[i] == 1 || gmap_bucket[i] == 1)
            {
                for (u32 j = 0; j < subbuckets_T[i]; j++)
                {
                    for (auto it = delT[i][j]->begin(); it != delT[i][j]->end(); it++)
                    {
                        Relation0Map* it2 = it->second;
                        for (auto dit2 = it2->begin(); dit2 != it2->end(); dit2++)
                        {
                            dt[0] = it->first;
                            dt[1] = dit2->first;
                            delT_temp[i].push_back(dt);
                        }
                    }
                }

                //
                int buffer_size = delT_temp[i].size() * COL_COUNT;
                //std::cout << "buffer size " << buffer_size << std::endl;
                u32* size_buf = new u32[tmap_distinct_sub_bucket_rank_count[i]];
                memset(size_buf, 0, sizeof(u32) * tmap_distinct_sub_bucket_rank_count[i]);

                u32 req_counter1 = 0;
                MPI_Request *req1 = new MPI_Request[gmap_distinct_sub_bucket_rank_count[i] + tmap_distinct_sub_bucket_rank_count[i]];
                MPI_Status *stat1 = new MPI_Status[gmap_distinct_sub_bucket_rank_count[i] + tmap_distinct_sub_bucket_rank_count[i]];

                // meta-data
                if (tmap[i] == 1)
                {
                    for (int r = 0; r < gmap_distinct_sub_bucket_rank_count[i]; r++)
                    {
                        //std::cout << "Sending " << buffer_size << " to " << gmap_distinct_sub_bucket_rank[i][r] << std::endl;
                        MPI_Isend(&buffer_size, 1, MPI_INT, gmap_distinct_sub_bucket_rank[i][r], 123, MPI_COMM_WORLD, &req1[req_counter1]);
                        req_counter1++;
                    }
                }

                if (gmap_bucket[i] == 1)
                {
                    for (int r = 0; r < tmap_distinct_sub_bucket_rank_count[i]; r++)
                    {
                        //std::cout << "Recieving " << "from " << tmap_distinct_sub_bucket_rank[i][r] << std::endl;
                        MPI_Irecv(size_buf + r, 1, MPI_INT, tmap_distinct_sub_bucket_rank[i][r], 123, MPI_COMM_WORLD, &req1[req_counter1]);
                        req_counter1++;
                    }
                }

                MPI_Waitall(req_counter1, req1, stat1);
                //std::cout << "Pass XXXXXXXXX " << i << std::endl;

                for (int r = 0; r < tmap_distinct_sub_bucket_rank_count[i]; r++)
                    total_buffer_size[i] = total_buffer_size[i] + size_buf[r];
                //

#if 1
                u32 req_counter2 = 0;
                MPI_Request *req2 = new MPI_Request[gmap_distinct_sub_bucket_rank_count[i] + tmap_distinct_sub_bucket_rank_count[i]];
                MPI_Status *stat2 = new MPI_Status[gmap_distinct_sub_bucket_rank_count[i] + tmap_distinct_sub_bucket_rank_count[i]];


                // data
                if (tmap[i] == 1)
                {
                    for (int r = 0; r < gmap_distinct_sub_bucket_rank_count[i]; r++)
                    {
                        //if (delT_temp[i].size() != 0)
                        {
                            MPI_Isend(&delT_temp[i][0], delT_temp[i].size() * COL_COUNT, MPI_UNSIGNED_LONG_LONG, gmap_distinct_sub_bucket_rank[i][r], 123, MPI_COMM_WORLD, &req2[req_counter2]);
                            req_counter2++;
                        }
                    }
                }

                u32 offset = 0;
                recvbuf[i] = new u64[total_buffer_size[i]];
                if (gmap_bucket[i] == 1)
                {
                    for (int r = 0; r < tmap_distinct_sub_bucket_rank_count[i]; r++)
                    {
                        //if (size_buf[r] != 0)
                        {
                            MPI_Irecv(recvbuf[i] + offset, size_buf[r], MPI_UNSIGNED_LONG_LONG, tmap_distinct_sub_bucket_rank[i][r], 123, MPI_COMM_WORLD, &req2[req_counter2]);
                            offset = offset + size_buf[r];
                            req_counter2++;
                        }
                    }
                }

                MPI_Waitall(req_counter2, req2, stat2);

                delete[] req1;
                delete[] req2;

                delete[] stat1;
                delete[] stat2;
                delete[] size_buf;


                Relation1Map tempT;
                for (u32 k1 = 0; k1 < total_buffer_size[i]; k1=k1+2)
                {
                    for (u32 j = 0; j < subbuckets_G[i]; j++)
                    {
                        auto itd = G[i][j].find(recvbuf[i][k1+1]);
                        if( itd != G[i][j].end() ) {
                            Relation0Map* Git = itd->second;
                            for (auto it2 = Git->begin(); it2 != Git->end(); it2++)
                            {
                                tuple_count++;

                                auto itx = tempT.find(recvbuf[i][k1]);
                                if( itx != tempT.end() ) {
                                    auto it2x = (itx->second)->find(it2->first);
                                    if( it2x != (itx->second)->end() ) {
                                        ;
                                    }
                                    else{
                                        (itx->second)->insert(std::make_pair(it2->first, 0));
                                        tempT[recvbuf[i][k1]] = itx->second;

                                        dt[0] = recvbuf[i][k1];
                                        dt[1] = it2->first;

                                        // because we are sending to T and dt and they are hashed on the second column
                                        uint64_t bucket_id = outer_hash(dt[1]) % buckets;
                                        uint64_t sub_bucket_id = outer_hash(dt[0]) % subbuckets_T[bucket_id];

                                        //int index = tmap_sub_bucket_rank[bucket_id][sub_bucket_id];
                                        uint64_t index = (outer_hash((bucket_id << 32)^sub_bucket_id))%nprocs;
                                        process_data_vector[index].push_back(dt);
                                    }
                                }
                                else {
                                    Relation0Map* k = new Relation0Map();
                                    k->insert(std::make_pair(it2->first, 0));
                                    //tempT.insert(std::make_pair(it->first,k));
                                    tempT[recvbuf[i][k1]] = k;

                                    dt[0] = recvbuf[i][k1];
                                    dt[1] = it2->first;

                                    // because we are sending to T and dt and they are hashed on the second column
                                    uint64_t bucket_id = outer_hash(dt[1]) % buckets;
                                    uint64_t sub_bucket_id = outer_hash(dt[0]) % subbuckets_T[bucket_id];

                                    uint64_t index = (outer_hash((bucket_id << 32)^sub_bucket_id))%nprocs;
                                    //int index = tmap_sub_bucket_rank[bucket_id][sub_bucket_id];
                                    process_data_vector[index].push_back(dt);
                                }
                            }
                        }
                    }
                }

                Relation1Map::iterator ix = tempT.begin();
                for(; ix != tempT.end(); ix++)
                    delete (ix->second);
                delete[]  recvbuf[i];
#endif
            }
        }
#endif
        double t1_e = MPI_Wtime();
        outer_comm_time = (t1_e - t1_s);


        double t2_s = MPI_Wtime();

        delete[]  recvbuf;
        delete[]  total_buffer_size;
        double t2_e = MPI_Wtime();
        join_time = (t2_e - t2_s);
    }

#if 1

#if 1
    double c1 = MPI_Wtime();

    int prefix_sum_process_size[nprocs];
    memset(prefix_sum_process_size, 0, nprocs * sizeof(int));

    int process_size[nprocs];
    memset(process_size, 0, nprocs * sizeof(int));

    u64 non_deduplicate_tuple_count = 0;
    process_size[0] = 2 * process_data_vector[0].size();
    non_deduplicate_tuple_count = non_deduplicate_tuple_count + process_data_vector[0].size();
    for (u32 i = 1; i < nprocs; i++)
    {
        prefix_sum_process_size[i] = prefix_sum_process_size[i - 1] + (2 * process_data_vector[i - 1].size());
        process_size[i] = 2 * process_data_vector[i].size();
        non_deduplicate_tuple_count = non_deduplicate_tuple_count + process_data_vector[i].size();
    }

    int process_data_buffer_size = prefix_sum_process_size[nprocs - 1] + process_size[nprocs - 1];

    u64* process_data = 0;
    process_data = new u64[process_data_buffer_size];
    memset(process_data, 0, process_data_buffer_size * sizeof(u64));

    for(u32 i = 0; i < nprocs; i++)
        memcpy(process_data + prefix_sum_process_size[i], &process_data_vector[i][0], process_data_vector[i].size() * sizeof(tuple<2>));

    delete[] process_data_vector;

    /* This step prepares for actual data transfer */
    /* Every process sends to every other process the amount of data it is going to send */

    int recv_process_size_buffer[nprocs];
    memset(recv_process_size_buffer, 0, nprocs * sizeof(int));
    MPI_Alltoall(process_size, 1, MPI_INT, recv_process_size_buffer, 1, MPI_INT, comm);

    int prefix_sum_recv_process_size_buffer[nprocs];
    memset(prefix_sum_recv_process_size_buffer, 0, nprocs * sizeof(int));

    /* Sending data to all processes: What is the buffer size to allocate */
    u32 outer_hash_buffer_size = recv_process_size_buffer[0];
    for(u32 i = 1; i < nprocs; i++)
    {
        prefix_sum_recv_process_size_buffer[i] = prefix_sum_recv_process_size_buffer[i - 1] + recv_process_size_buffer[i - 1];
        outer_hash_buffer_size = outer_hash_buffer_size + recv_process_size_buffer[i];
    }

    u64 *hash_buffer = 0;
    hash_buffer = new u64[outer_hash_buffer_size];
    memset(hash_buffer, 0, outer_hash_buffer_size * sizeof(u64));

#if 1
    MPI_Alltoallv(process_data, process_size, prefix_sum_process_size, MPI_UNSIGNED_LONG_LONG, hash_buffer, recv_process_size_buffer, prefix_sum_recv_process_size_buffer, MPI_UNSIGNED_LONG_LONG, comm);
#endif

#if 1
    double c2 = MPI_Wtime();
    double global_comm_time = (c2-c1);

    double i1 = MPI_Wtime();
    u64 tduplicates = 0;
    int tcount = 0;
    u64 count = 0;
    memset(dtmap, 0, buckets * sizeof(u32));
    for (u32 i = 0; i < outer_hash_buffer_size; i = i + 2)
    {
        uint64_t bucket_id = outer_hash(hash_buffer[i + 1]) % buckets;
        uint64_t sub_bucket_id = outer_hash(hash_buffer[i]) % subbuckets_T[bucket_id];

        auto it = T[bucket_id][sub_bucket_id].find(hash_buffer[i]);
        if ( it != T[bucket_id][sub_bucket_id].end() ) {
            auto it2 = (it->second)->find(hash_buffer[i + 1]);
            if ( it2 != (it->second)->end() ) {
                tduplicates++;
            }
            else{
                (it->second)->insert(std::make_pair(hash_buffer[i + 1], 0));
                T[bucket_id][sub_bucket_id].insert(std::make_pair(hash_buffer[i], it->second));

                tcount++;
                auto itx = delTT[bucket_id][sub_bucket_id]->find(hash_buffer[i]);
                if ( itx != delTT[bucket_id][sub_bucket_id]->end() ) {
                    auto it2x = (itx->second)->find(hash_buffer[i + 1]);
                    if ( it2x != (itx->second)->end() ) {
                        ;
                    }
                    else{
                        (itx->second)->insert(std::make_pair(hash_buffer[i + 1], 0));
                        delTT[bucket_id][sub_bucket_id]->insert(std::make_pair(hash_buffer[i], itx->second));
                        dtmap[bucket_id] = 1;
                        count++;
                    }
                }
                else {
                    Relation0Map *k = new Relation0Map;
                    k->insert(std::make_pair(hash_buffer[i + 1], 0));
                    delTT[bucket_id][sub_bucket_id]->insert(std::make_pair(hash_buffer[i],k));
                    dtmap[bucket_id] = 1;
                    count++;
                }
            }
        }
        else {
            Relation0Map *k = new Relation0Map;
            k->insert(std::make_pair(hash_buffer[i + 1], 0));
            T[bucket_id][sub_bucket_id].insert(std::make_pair(hash_buffer[i],k));
            tcount++;

            auto itx = delTT[bucket_id][sub_bucket_id]->find(hash_buffer[i]);
            if ( itx != delTT[bucket_id][sub_bucket_id]->end() ) {
                auto it2x = (itx->second)->find(hash_buffer[i + 1]);
                if ( it2x != (itx->second)->end() ) {
                    ;
                }
                else{
                    (itx->second)->insert(std::make_pair(hash_buffer[i + 1], 0));
                    delTT[bucket_id][sub_bucket_id]->insert(std::make_pair(hash_buffer[i], itx->second));
                    dtmap[bucket_id] = 1;
                    count++;
                }
            }
            else {
                Relation0Map *k = new Relation0Map;
                k->insert(std::make_pair(hash_buffer[i + 1], 0));
                delTT[bucket_id][sub_bucket_id]->insert(std::make_pair(hash_buffer[i],k));
                dtmap[bucket_id] = 1;
                count++;
            }
        }
    }
#endif
#endif

    delete[] hash_buffer;
    delete[] process_data;
    double i2 = MPI_Wtime();
    double insert_time = (i2 - i1);

#if 1

    int global_sum = 0;
    MPI_Allreduce(&count, &global_sum, 1, MPI_INT, MPI_SUM, comm);
    //if (lc == 1)
    //    std::cout << "Rank : " << rank  << " Count " << count << std::endl;


    double v1 = MPI_Wtime();
    if (lc % 10 == 0)
    {
        int sum = 0;
        MPI_Allreduce(&count, &sum, 1, MPI_INT, MPI_BOR, comm);
        if(sum == 0)
            *lb = 1;
        else
            *lb = 0;
    }

    //
    *running_t_count = *running_t_count + tcount;
    double v2 = MPI_Wtime();


    *running_time = *running_time + (v2 - comm_create_start);

    if (rank == 0)
        std::cout << lc << " [" << v2-comm_create_start << "] [" << *running_time << "]"
                  << " T t: " << global_sum
                  << " N t " << tuple_count
                  << " non dup: " << non_deduplicate_tuple_count
                  << " Comm: " << (comm_create_end - comm_create_start)
                  << " Outer: " << outer_comm_time
                  << " Join: " << join_time
                  << " Comm: " << global_comm_time
                  << " Ins: " << insert_time
                  << " Ver: " << (v2 - v1)
                  << " Delta: " << tcount
                  << " Dup Delta: " << tduplicates
                  << " T : " << *running_t_count
                  << std::endl;

    for (u32 i = 0; i < buckets; i++)
    {
        for (u32 j = 0; j < subbuckets_T[i]; j++)
        {
            Relation1Map::iterator iy = delT[i][j]->begin();
            for(; iy != delT[i][j]->end(); iy++)
                delete (iy->second);
            delete delT[i][j];
        }
        delete[] delT[i];
    }
    delete[] delT;


#endif
#endif
    return delTT;
}


void load_balance_G(u32 buckets, u32* gmap_bucket, u32** gmap_sub_bucket, u32* subbuckets_G, Relation1Map **G, int** gmap_sub_bucket_rank, int* gmap_distinct_sub_bucket_rank_count, int** gmap_distinct_sub_bucket_rank)
{
    u64 G_tuple_count_before = 0;
    u64 G_tuple_count_before_global = 0;
    for (u32 bk = 0; bk < buckets; bk++)
    {
        for (u32 j = 0; j < subbuckets_G[bk]; j++)
        {
            for (auto it = G[bk][j].begin(); it != G[bk][j].end(); it++)
            {
                Relation0Map* it2 = it->second;
                for (auto dit2 = it2->begin(); dit2 != it2->end(); dit2++)
                {
                    G_tuple_count_before++;
                }
            }
        }
    }
    MPI_Allreduce(&G_tuple_count_before, &G_tuple_count_before_global, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
    if (rank == 0)
        std::cout << "Tuples before load balancing: " << G_tuple_count_before_global;


    u32 max_sub_bucket_size = 0;
    u32 min_sub_bucket_size = INT_MAX;
    for (u32 i = 0; i < buckets; i++)
    {
        for (u32 j = 0; j < subbuckets_G[i]; j++)
        {
            if (gmap_sub_bucket[i][j] != 0)
            {
                if (gmap_sub_bucket[i][j] > max_sub_bucket_size)
                    max_sub_bucket_size = gmap_sub_bucket[i][j];

                if (gmap_sub_bucket[i][j] < min_sub_bucket_size)
                    min_sub_bucket_size = gmap_sub_bucket[i][j];
            }
        }
    }

    int global_max;
    int global_min;
    //u64 total_outer_hash_buffer_size = 0;
    MPI_Allreduce(&max_sub_bucket_size, &global_max, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    MPI_Allreduce(&min_sub_bucket_size, &global_min, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);

    u32 g_new_sub_bucket[buckets];
    memset(g_new_sub_bucket, 0, buckets * sizeof(u32));

    u32 global_g_new_sub_bucket[buckets];
    memset(global_g_new_sub_bucket, 0, buckets * sizeof(u32));
    for (u32 i = 0; i < buckets; i++)
        for (u32 j = 0; j < subbuckets_G[i]; j++)
            g_new_sub_bucket[i] = g_new_sub_bucket[i] + (gmap_sub_bucket[i][j] / global_min);

    MPI_Allreduce(g_new_sub_bucket, global_g_new_sub_bucket, buckets, MPI_INT, MPI_SUM, MPI_COMM_WORLD);


    int rcount = 0;
    for (u64 b = 0; b < buckets; b++)
    {
        if (global_g_new_sub_bucket[b] != subbuckets_G[b])
        {
            gmap_bucket[b] = 0;
            std::unordered_set<int> distinct_g_ranks;
            delete[] gmap_sub_bucket_rank[b];
            gmap_sub_bucket_rank[b] = new int[global_g_new_sub_bucket[b]];

            for (u64 x = 0; x < global_g_new_sub_bucket[b]; x++)
            {
                gmap_sub_bucket_rank[b][x] = (outer_hash((b << 32)^x))%nprocs;//rcount % nprocs;

                if (gmap_sub_bucket_rank[b][x] == rank)
                    gmap_bucket[b] = 1;

                distinct_g_ranks.insert(gmap_sub_bucket_rank[b][x]);
                rcount++;
            }

            delete[] gmap_distinct_sub_bucket_rank[b];
            gmap_distinct_sub_bucket_rank[b] = new int[distinct_g_ranks.size()];
            u32 x = 0;
            for ( auto it = distinct_g_ranks.begin(); it != distinct_g_ranks.end(); ++it )
            {
                gmap_distinct_sub_bucket_rank[b][x] = *it;
                x++;
            }
            gmap_distinct_sub_bucket_rank_count[b] = x;
        }
    }

    /* process_size[j] stores the number of samples to be sent to process with rank j */
    int process_size[nprocs];
    memset(process_size, 0, nprocs * sizeof(int));

    /* vector[i] contains the data that needs to be sent to process i */
    std::vector<u64> process_data_vector[nprocs];

    int prefix_sum_process_size[nprocs];
    memset(prefix_sum_process_size, 0, nprocs * sizeof(int));

    int recv_process_size_buffer[nprocs];
    memset(recv_process_size_buffer, 0, nprocs * sizeof(int));

    u64* process_data;
    int process_data_buffer_size = 0;

    for (u32 i = 0; i < buckets; i++)
    {
        if (global_g_new_sub_bucket[i] != subbuckets_G[i])
        {
            delete[] gmap_sub_bucket[i];
            gmap_sub_bucket[i] = new u32[global_g_new_sub_bucket[i]];
            memset(gmap_sub_bucket[i], 0, sizeof(u32) * global_g_new_sub_bucket[i]);

            for (u32 j = 0; j < subbuckets_G[i]; j++)
            {
                for (auto it = G[i][j].begin(); it != G[i][j].end(); it++)
                {
                    Relation0Map* it2 = it->second;
                    for (auto dit2 = it2->begin(); dit2 != it2->end(); dit2++)
                    {
                        uint64_t bucket_id = outer_hash(it->first) % buckets;
                        uint64_t sub_bucket_id = outer_hash(dit2->first) % global_g_new_sub_bucket[bucket_id];

                        //uint64_t index = (outer_hash((bucket_id << 32)^sub_bucket_id))%nprocs;
                        int index = gmap_sub_bucket_rank[bucket_id][sub_bucket_id];
                        process_size[index] = process_size[index] + COL_COUNT;

                        process_data_vector[index].push_back(it->first);
                        process_data_vector[index].push_back(dit2->first);
                    }
                }

                Relation1Map::iterator iy2 = G[i][j].begin();
                for(; iy2 != G[i][j].end(); iy2++)
                    delete (iy2->second);

                G[i][j].clear();

            }

            delete[] G[i];
            G[i] = new Relation1Map[global_g_new_sub_bucket[i]];
        }
    }


    //if (rank == 1)
    //    std::cout << "process_size[0] " << process_size[0] << " process_size[1] " << process_size[1] << std::endl;

    for (u32 ix = 1; ix < nprocs; ix++)
        prefix_sum_process_size[ix] = prefix_sum_process_size[ix - 1] + process_size[ix - 1];

    process_data_buffer_size = prefix_sum_process_size[nprocs - 1] + process_size[nprocs - 1];

    process_data = new u64[process_data_buffer_size];

    for (u32 ix = 0; ix < nprocs; ix++)
        memcpy(process_data + prefix_sum_process_size[ix], &process_data_vector[ix][0], process_data_vector[ix].size() * sizeof(u64));


    MPI_Alltoall(process_size, 1, MPI_INT, recv_process_size_buffer, 1, MPI_INT, MPI_COMM_WORLD);

#if 1
    int prefix_sum_recv_process_size_buffer[nprocs];
    memset(prefix_sum_recv_process_size_buffer, 0, nprocs * sizeof(int));
    for (u32 ix = 1; ix < nprocs; ix++)
        prefix_sum_recv_process_size_buffer[ix] = prefix_sum_recv_process_size_buffer[ix - 1] + recv_process_size_buffer[ix - 1];


    int outer_hash_buffer_size = 0;
    for(u32 ix = 0; ix < nprocs; ix++)
        outer_hash_buffer_size = outer_hash_buffer_size + recv_process_size_buffer[ix];


    u64* outer_hash_data = new u64[outer_hash_buffer_size];
    MPI_Alltoallv(process_data, process_size, prefix_sum_process_size, MPI_UNSIGNED_LONG_LONG, outer_hash_data, recv_process_size_buffer, prefix_sum_recv_process_size_buffer, MPI_UNSIGNED_LONG_LONG, comm);

    if (process_data_buffer_size != 0)
        delete[] process_data;


    //subbuckets_G[i] = global_g_new_sub_bucket[i];
    for (int in = 0; in < outer_hash_buffer_size; in = in + 2)
    {
        uint64_t bucket_id = outer_hash((outer_hash_data)[in]) % buckets;
        uint64_t sub_bucket_id = outer_hash((outer_hash_data)[in+1]) % global_g_new_sub_bucket[bucket_id];
        gmap_bucket[bucket_id] = 1;
        gmap_sub_bucket[bucket_id][sub_bucket_id]++;

        //if (rank == 1)
        //std::cout << "Val " << outer_hash_data[in] << " b " << bucket_id << " sbi " << sub_bucket_id << std::endl;

        auto it = G[bucket_id][sub_bucket_id].find((outer_hash_data)[in]);
        if( it != G[bucket_id][sub_bucket_id].end() ) {
            auto it2 = (it->second)->find((outer_hash_data)[in+1]);
            if( it2 != (it->second)->end() ) {
                ;
            }
            else{
                (it->second)->insert(std::make_pair((outer_hash_data)[in + 1], 0));
                G[bucket_id][sub_bucket_id][(outer_hash_data)[in]] = it->second;
            }
        }
        else {
            Relation0Map *k = new Relation0Map;
            k->insert(std::make_pair((outer_hash_data)[in + 1], 0));
            G[bucket_id][sub_bucket_id].insert(std::make_pair((outer_hash_data)[in],k));
        }
    }

    delete[] outer_hash_data;
#endif

    //
    if (rank == 0)
    {
        for (u32 i = 0; i < buckets; i++)
            std::cout << i << "[G] Bucket count changed from " << subbuckets_G[i] << " to " << global_g_new_sub_bucket[i] << std::endl;

        //for (u32 i = 0; i < buckets; i++)
        //    for (u32 j = 0; j < global_g_new_sub_bucket[i]; j++)
        //        std::cout << "[After] Element count at " << i << " " << j << " is " << gmap_sub_bucket[i][j] << std::endl;
    }
    //

    memcpy(subbuckets_G, global_g_new_sub_bucket, sizeof(u32)*buckets);

    u64 G_tuple_count_after = 0;
    u64 G_tuple_count_after_global = 0;
    for (u32 bk = 0; bk < buckets; bk++)
    {
        for (u32 j = 0; j < subbuckets_G[bk]; j++)
        {
            for (auto it = G[bk][j].begin(); it != G[bk][j].end(); it++)
            {
                Relation0Map* it2 = it->second;
                for (auto dit2 = it2->begin(); dit2 != it2->end(); dit2++)
                    G_tuple_count_after++;
            }
        }
    }
    MPI_Allreduce(&G_tuple_count_after, &G_tuple_count_after_global, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);


    if (rank == 0)
        std::cout << "[G] Before and After " << G_tuple_count_before_global << " " << G_tuple_count_after_global << std::endl;

    //assert(G_tuple_count_before_global == G_tuple_count_after_global);

    //std::cout << "Balanced [G] Rank: " << rank << " Hashed count: " << outer_hash_buffer_size/2 << " Ratio: " << (float)((float) (outer_hash_buffer_size/2)/global_row_count)*100 << std::endl;
}



void load_balance_T(u32 buckets, u32* tmap_bucket, u32** tmap_sub_bucket, u32* subbuckets_T, Relation1Map **T, u32* dtmap_bucket, u32** dtmap_sub_bucket, Relation1Map ***delta, int** tmap_sub_bucket_rank, int* tmap_distinct_sub_bucket_rank_count, int** tmap_distinct_sub_bucket_rank)
{
    u64 dT_tuple_count_before = 0;
    u64 T_tuple_count_before = 0;
    u64 dT_tuple_count_before_global = 0;
    u64 T_tuple_count_before_global = 0;
    for (u32 bk = 0; bk < buckets; bk++)
    {
        for (u32 j = 0; j < subbuckets_T[bk]; j++)
        {
            for (auto it = T[bk][j].begin(); it != T[bk][j].end(); it++)
            {
                Relation0Map* it2 = it->second;
                for (auto dit2 = it2->begin(); dit2 != it2->end(); dit2++)
                {
                    T_tuple_count_before++;
                }
            }

            for (auto it = delta[bk][j]->begin(); it != delta[bk][j]->end(); it++)
            {
                Relation0Map* it2 = it->second;
                for (auto dit2 = it2->begin(); dit2 != it2->end(); dit2++)
                {
                    dT_tuple_count_before++;
                }
            }
        }
    }
    MPI_Allreduce(&T_tuple_count_before, &T_tuple_count_before_global, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&dT_tuple_count_before, &dT_tuple_count_before_global, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);

    if (rank == 0)
    {
        std::cout << "[Before] T size " << T_tuple_count_before_global << std::endl;
        std::cout << "[Before] dT size " << dT_tuple_count_before_global << std::endl;
    }

    int col_count = 2;
    u32 max_sub_bucket_size = 0;
    u32 min_sub_bucket_size = INT_MAX;


    for (u32 i = 0; i < buckets; i++)
    {
        for (u32 j = 0; j < subbuckets_T[i]; j++)
        {
            if (tmap_sub_bucket[i][j] != 0)
            {
                if (tmap_sub_bucket[i][j] > max_sub_bucket_size)
                    max_sub_bucket_size = tmap_sub_bucket[i][j];

                if (tmap_sub_bucket[i][j] < min_sub_bucket_size)
                    min_sub_bucket_size = tmap_sub_bucket[i][j];
            }
        }
    }

    int global_max;
    int global_min;
    MPI_Allreduce(&max_sub_bucket_size, &global_max, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    MPI_Allreduce(&min_sub_bucket_size, &global_min, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);

    u32 t_new_sub_bucket[buckets];
    memset(t_new_sub_bucket, 0, buckets * sizeof(u32));

    u32 global_t_new_sub_bucket[buckets];
    memset(global_t_new_sub_bucket, 0, buckets * sizeof(u32));
    for (u32 i = 0; i < buckets; i++)
        for (u32 j = 0; j < subbuckets_T[i]; j++)
            t_new_sub_bucket[i] = t_new_sub_bucket[i] + (tmap_sub_bucket[i][j] / global_min);

    MPI_Allreduce(t_new_sub_bucket, global_t_new_sub_bucket, buckets, MPI_INT, MPI_SUM, MPI_COMM_WORLD);


    int rcount = 0;
    for (u64 b = 0; b < buckets; b++)
    {
        if (subbuckets_T[b] != global_t_new_sub_bucket[b])
        {
            tmap_bucket[b] = 0;
            dtmap_bucket[b] = 0;

            std::unordered_set<int> distinct_t_ranks;
            delete[] tmap_sub_bucket_rank[b];
            tmap_sub_bucket_rank[b] = new int[global_t_new_sub_bucket[b]];

            for (u64 x = 0; x < global_t_new_sub_bucket[b]; x++)
            {
                tmap_sub_bucket_rank[b][x] = (outer_hash((b << 32)^x))%nprocs;//rcount % nprocs;

                if (tmap_sub_bucket_rank[b][x] == rank)
                    tmap_bucket[b] = 1;

                distinct_t_ranks.insert(tmap_sub_bucket_rank[b][x]);
                rcount++;
            }

            delete[] tmap_distinct_sub_bucket_rank[b];
            tmap_distinct_sub_bucket_rank[b] = new int[distinct_t_ranks.size()];
            u32 x = 0;
            for ( auto it = distinct_t_ranks.begin(); it != distinct_t_ranks.end(); ++it )
            {
                tmap_distinct_sub_bucket_rank[b][x] = *it;
                x++;
            }
            tmap_distinct_sub_bucket_rank_count[b] = x;
        }
    }


#if 1

    /* process_size[j] stores the number of samples to be sent to process with rank j */
    int process_size[nprocs];
    memset(process_size, 0, nprocs * sizeof(int));

    /* vector[i] contains the data that needs to be sent to process i */
    std::vector<u64> process_data_vector[nprocs];

    int prefix_sum_process_size[nprocs];
    memset(prefix_sum_process_size, 0, nprocs * sizeof(int));

    int recv_process_size_buffer[nprocs];
    memset(recv_process_size_buffer, 0, nprocs * sizeof(int));

    u64* process_data;

    for (u32 bk = 0; bk < buckets; bk++)
    {
        if (subbuckets_T[bk] != global_t_new_sub_bucket[bk])
        {
            delete[] tmap_sub_bucket[bk];
            tmap_sub_bucket[bk] = new u32[global_t_new_sub_bucket[bk]];
            memset(tmap_sub_bucket[bk], 0, sizeof(u32) * global_t_new_sub_bucket[bk]);

            for (u32 j = 0; j < subbuckets_T[bk]; j++)
            {
                for (auto it = T[bk][j].begin(); it != T[bk][j].end(); it++)
                {
                    Relation0Map* it2 = it->second;
                    for (auto dit2 = it2->begin(); dit2 != it2->end(); dit2++)
                    {
                        //test
                        uint64_t bucket_id = outer_hash(dit2->first) % buckets;
                        uint64_t sub_bucket_id = outer_hash(it->first) % global_t_new_sub_bucket[bucket_id];

                        uint64_t index = (outer_hash((bucket_id << 32)^sub_bucket_id))%nprocs;
                        //int index = tmap_sub_bucket_rank[bucket_id][sub_bucket_id];
                        process_size[index] = process_size[index] + col_count;

                        process_data_vector[index].push_back(it->first);
                        process_data_vector[index].push_back(dit2->first);
                    }
                }
                Relation1Map::iterator iy2 = T[bk][j].begin();
                for(; iy2 != T[bk][j].end(); iy2++)
                    delete (iy2->second);

                T[bk][j].clear();
            }
            delete[] T[bk];
            T[bk] = new Relation1Map[global_t_new_sub_bucket[bk]];
        }
    }

    for (u32 ix = 1; ix < nprocs; ix++)
        prefix_sum_process_size[ix] = prefix_sum_process_size[ix - 1] + process_size[ix - 1];

    int process_data_buffer_size = prefix_sum_process_size[nprocs - 1] + process_size[nprocs - 1];

    //u64 process_data[process_data_buffer_size];
    process_data = new u64[process_data_buffer_size];

    for (u32 ix = 0; ix < nprocs; ix++)
        memcpy(process_data + prefix_sum_process_size[ix], &process_data_vector[ix][0], process_data_vector[ix].size() * sizeof(u64));


    /* This step prepares for actual data transfer */
    /* Every process sends to every other process the amount of data it is going to send */


    MPI_Alltoall(process_size, 1, MPI_INT, recv_process_size_buffer, 1, MPI_INT, MPI_COMM_WORLD);


    int prefix_sum_recv_process_size_buffer[nprocs];
    memset(prefix_sum_recv_process_size_buffer, 0, nprocs * sizeof(int));
    for (u32 ix = 1; ix < nprocs; ix++)
        prefix_sum_recv_process_size_buffer[ix] = prefix_sum_recv_process_size_buffer[ix - 1] + recv_process_size_buffer[ix - 1];


    /* Sending data to all processes */
    /* What is the buffer size to allocate */
    int outer_hash_buffer_size = 0;
    for(u32 ix = 0; ix < nprocs; ix++)
        outer_hash_buffer_size = outer_hash_buffer_size + recv_process_size_buffer[ix];


    u64* outer_hash_data = new u64[outer_hash_buffer_size];
    MPI_Alltoallv(process_data, process_size, prefix_sum_process_size, MPI_UNSIGNED_LONG_LONG, outer_hash_data, recv_process_size_buffer, prefix_sum_recv_process_size_buffer, MPI_UNSIGNED_LONG_LONG, comm);

    if (process_data_buffer_size != 0)
        delete[] process_data;

#if 1
    //std::cout << "outer_hash_buffer_size " << outer_hash_buffer_size << std::endl;
    //subbuckets_G[i] = global_t_new_sub_bucket[i];
    for (int in = 0; in < outer_hash_buffer_size; in = in + 2)
    {
        uint64_t bucket_id = outer_hash((outer_hash_data)[in + 1]) % buckets;
        uint64_t sub_bucket_id = outer_hash((outer_hash_data)[in]) % global_t_new_sub_bucket[bucket_id];
        tmap_bucket[bucket_id] = 1;
        tmap_sub_bucket[bucket_id][sub_bucket_id]++;

        auto it = T[bucket_id][sub_bucket_id].find((outer_hash_data)[in]);
        if( it != T[bucket_id][sub_bucket_id].end() ) {
            auto it2 = (it->second)->find((outer_hash_data)[in+1]);
            if( it2 != (it->second)->end() ) {
                ;
            }
            else{
                (it->second)->insert(std::make_pair((outer_hash_data)[in + 1], 0));
                T[bucket_id][sub_bucket_id][(outer_hash_data)[in]] = it->second;
            }
        }
        else {
            Relation0Map *k = new Relation0Map;
            k->insert(std::make_pair((outer_hash_data)[in + 1], 0));
            T[bucket_id][sub_bucket_id].insert(std::make_pair((outer_hash_data)[in],k));
        }
    }

    delete[] outer_hash_data;
#endif


#endif

#if 1

    /* process_size[j] stores the number of samples to be sent to process with rank j */
    int process_size_dt[nprocs];
    memset(process_size_dt, 0, nprocs * sizeof(int));

    /* vector[i] contains the data that needs to be sent to process i */
    std::vector<u64> process_data_vector_dt[nprocs];

    int prefix_sum_process_size_dt[nprocs];
    memset(prefix_sum_process_size_dt, 0, nprocs * sizeof(int));

    int recv_process_size_buffer_dt[nprocs];
    memset(recv_process_size_buffer_dt, 0, nprocs * sizeof(int));

    u64* process_data_dt;

    for (u32 bk = 0; bk < buckets; bk++)
    {
        if (subbuckets_T[bk] != global_t_new_sub_bucket[bk])
        {
            delete[] dtmap_sub_bucket[bk];
            dtmap_sub_bucket[bk] = new u32[global_t_new_sub_bucket[bk]];
            memset(dtmap_sub_bucket[bk], 0, sizeof(u32) * global_t_new_sub_bucket[bk]);

            for (u32 j = 0; j < subbuckets_T[bk]; j++)
            {
                for (auto it = delta[bk][j]->begin(); it != delta[bk][j]->end(); it++)
                {
                    Relation0Map* it2 = it->second;
                    for (auto dit2 = it2->begin(); dit2 != it2->end(); dit2++)
                    {
                        uint64_t bucket_id = outer_hash(dit2->first) % buckets;
                        uint64_t sub_bucket_id = outer_hash(it->first) % global_t_new_sub_bucket[bucket_id];

                        //int index = tmap_sub_bucket_rank[bucket_id][sub_bucket_id];
                        uint64_t index = (outer_hash((bucket_id << 32)^sub_bucket_id))%nprocs;
                        process_size_dt[index] = process_size_dt[index] + col_count;

                        process_data_vector_dt[index].push_back(it->first);
                        process_data_vector_dt[index].push_back(dit2->first);
                    }
                }

                Relation1Map::iterator iy2 = delta[bk][j]->begin();
                for(; iy2 != delta[bk][j]->end(); iy2++)
                    delete (iy2->second);

                delta[bk][j]->clear();
                delete delta[bk][j];
            }
            delete[] delta[bk];
            delta[bk] = new Relation1Map*[global_t_new_sub_bucket[bk]];
            for (u32 j = 0; j < global_t_new_sub_bucket[bk]; j++)
                delta[bk][j] = new Relation1Map;

        }
    }

    for (u32 ix = 1; ix < nprocs; ix++)
        prefix_sum_process_size_dt[ix] = prefix_sum_process_size_dt[ix - 1] + process_size_dt[ix - 1];

    int process_data_buffer_size_dt = prefix_sum_process_size_dt[nprocs - 1] + process_size_dt[nprocs - 1];

    //u64 process_data[process_data_buffer_size];
    process_data_dt = new u64[process_data_buffer_size_dt];

    for (u32 ix = 0; ix < nprocs; ix++)
        memcpy(process_data_dt + prefix_sum_process_size_dt[ix], &process_data_vector_dt[ix][0], process_data_vector_dt[ix].size() * sizeof(u64));


    /* This step prepares for actual data transfer */
    /* Every process sends to every other process the amount of data it is going to send */


    MPI_Alltoall(process_size_dt, 1, MPI_INT, recv_process_size_buffer_dt, 1, MPI_INT, MPI_COMM_WORLD);

#if 1
    int prefix_sum_recv_process_size_buffer_dt[nprocs];
    memset(prefix_sum_recv_process_size_buffer_dt, 0, nprocs * sizeof(int));
    for (u32 ix = 1; ix < nprocs; ix++)
        prefix_sum_recv_process_size_buffer_dt[ix] = prefix_sum_recv_process_size_buffer_dt[ix - 1] + recv_process_size_buffer_dt[ix - 1];


    /* Sending data to all processes */
    /* What is the buffer size to allocate */
    int outer_hash_buffer_size_dt = 0;
    for (u32 ix = 0; ix < nprocs; ix++)
        outer_hash_buffer_size_dt = outer_hash_buffer_size_dt + recv_process_size_buffer_dt[ix];


    u64* outer_hash_data_dt = new u64[outer_hash_buffer_size_dt];
    MPI_Alltoallv(process_data_dt, process_size_dt, prefix_sum_process_size_dt, MPI_UNSIGNED_LONG_LONG, outer_hash_data_dt, recv_process_size_buffer_dt, prefix_sum_recv_process_size_buffer_dt, MPI_UNSIGNED_LONG_LONG, comm);

    if (process_data_buffer_size_dt != 0)
        delete[] process_data_dt;


    //std::cout << "outer_hash_buffer_size " << outer_hash_buffer_size << std::endl;
    //subbuckets_G[i] = global_t_new_sub_bucket[i];
    for (int in = 0; in < outer_hash_buffer_size_dt; in = in + 2)
    {
        uint64_t bucket_id = outer_hash((outer_hash_data_dt)[in+1]) % buckets;
        uint64_t sub_bucket_id = outer_hash((outer_hash_data_dt)[in]) % global_t_new_sub_bucket[bucket_id];
        dtmap_bucket[bucket_id] = 1;
        dtmap_sub_bucket[bucket_id][sub_bucket_id]++;

        auto it = delta[bucket_id][sub_bucket_id]->find((outer_hash_data_dt)[in]);
        if( it != delta[bucket_id][sub_bucket_id]->end() ) {
            auto it2 = (it->second)->find((outer_hash_data_dt)[in+1]);
            if( it2 != (it->second)->end() ) {
                ;
            }
            else{
                (it->second)->insert(std::make_pair((outer_hash_data_dt)[in + 1], 0));
                delta[bucket_id][sub_bucket_id]->insert(std::make_pair((outer_hash_data_dt)[in], it->second));
            }
        }
        else {
            Relation0Map *k = new Relation0Map;
            k->insert(std::make_pair((outer_hash_data_dt)[in + 1], 0));
            delta[bucket_id][sub_bucket_id]->insert(std::make_pair((outer_hash_data_dt)[in],k));
        }
    }
#endif
    delete[] outer_hash_data_dt;


    if (rank == 0)
        for (u32 i = 0; i < buckets; i++)
            std::cout << "[T] Bucket count changed from " << subbuckets_T[i] << " to " << global_t_new_sub_bucket[i] << std::endl;

    memcpy(subbuckets_T, global_t_new_sub_bucket, sizeof(u32)*buckets);

    //std::cout << "Balanced [T] Rank: " << rank << " Hashed count: " << outer_hash_buffer_size << " Ratio: " << (float)((float) outer_hash_buffer_size/global_row_count)*100 << std::endl;

    u64 dT_tuple_count_after = 0;
    u64 T_tuple_count_after = 0;
    u64 dT_tuple_count_after_global = 0;
    u64 T_tuple_count_after_global = 0;
    for (u32 bk = 0; bk < buckets; bk++)
    {
        for (u32 j = 0; j < subbuckets_T[bk]; j++)
        {
            for (auto it = T[bk][j].begin(); it != T[bk][j].end(); it++)
            {
                Relation0Map* it2 = it->second;
                for (auto dit2 = it2->begin(); dit2 != it2->end(); dit2++)
                    T_tuple_count_after++;
            }

            for (auto it = delta[bk][j]->begin(); it != delta[bk][j]->end(); it++)
            {
                Relation0Map* it2 = it->second;
                for (auto dit2 = it2->begin(); dit2 != it2->end(); dit2++)
                {
                    dT_tuple_count_after++;
                    //if (rank == 0)
                    //    std::cout << " Val " << it->first << " " << dit2->first << std::endl;
                }
            }
        }
    }
    MPI_Allreduce(&T_tuple_count_after, &T_tuple_count_after_global, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&dT_tuple_count_after, &dT_tuple_count_after_global, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);

    if (rank == 0)
    {
        std::cout << "[T] Before and After " << T_tuple_count_before_global << " " << T_tuple_count_after_global << std::endl;
        std::cout << "[dT] Before and After " << dT_tuple_count_before_global << " " << dT_tuple_count_after_global << std::endl;
    }

    //assert(T_tuple_count_before_global == T_tuple_count_after_global);
    //assert(dT_tuple_count_before_global == dT_tuple_count_after_global);

    //std::cout << "Balanced [dT] Rank: " << rank << " Hashed count: " << total_outer_hash_buffer_size_dT << " Ratio: " << (float)((float) total_outer_hash_buffer_size_dT/global_row_count)*100 << std::endl;
#endif

}



void dump_relation(Relation1Map*** X, int lc, int buckets, u32* subbuckets, u32* map)
{

    for (int i = 0; i < buckets; i++)
    {

        if (map[i] == 1)
        {
            std::ofstream myfile;
            char filename[1024];
            sprintf(filename, "filename_%d_%d_%d", lc, rank, i);
            myfile.open (filename);

            for (u32 j = 0; j < subbuckets[i]; j++)
            {
                for ( auto local_it = X[i][j]->begin(); local_it!= X[i][j]->end(); ++local_it )
                {
                    Relation0Map* k = local_it->second;
                    for (auto it2 = k->begin(); it2 != k->end(); it2++)
                        myfile << local_it->first << " " << it2->first << std::endl;
                }
            }

            myfile.close();
        }
    }

}



int main(int argc, char **argv)
{
    // Initializing MPI
    MPI_Init(&argc, &argv);
    int size = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (argc != 5)
    {
        if (rank == 0)
        {
            std::cout << "Usage:" << "TC <bucket count factor> <initial sub bucket count> <comm type>"   << std::endl;
            MPI_Abort(MPI_COMM_WORLD, -1);
        }
    }

#if 1
    comm = MPI_COMM_WORLD;
    double ior_start = MPI_Wtime();

    nprocs = (u32)size;

    u32 buckets;
    u32 *subbuckets_G;
    u32 *subbuckets_T;

    // this is the initialization step, we can change this as well
    buckets = nprocs * atoi(argv[2]);

    // the relations can have different number of sub buckets
    subbuckets_G = new u32[buckets];
    memset(subbuckets_G, 0, buckets*sizeof(u32));

    subbuckets_T = new u32[buckets];
    memset(subbuckets_T, 0, buckets*sizeof(u32));

    for (u64 b = 0; b < buckets; b++)
    {
        subbuckets_G[b] = atoi(argv[3]);
        subbuckets_T[b] = atoi(argv[3]);
    }

    int** gmap_sub_bucket_rank = new int*[buckets];
    for (u64 b = 0; b < buckets; b++)
        gmap_sub_bucket_rank[b] = new int[subbuckets_G[b]];

    int** tmap_sub_bucket_rank = new int*[buckets];
    for (u64 b = 0; b < buckets; b++)
        tmap_sub_bucket_rank[b] = new int[subbuckets_T[b]];


    int** gmap_distinct_sub_bucket_rank = new int*[buckets];
    int* gmap_distinct_sub_bucket_rank_count = new int[buckets];


    u32 *gmap_bucket = new u32[buckets];
    memset(gmap_bucket, 0, sizeof(u32) * buckets);


    int rcount = 0;
    for (u64 b = 0; b < buckets; b++)
    {
        std::unordered_set<int> distinct_g_ranks;
        for (u64 x = 0; x < subbuckets_G[b]; x++)
        {
            gmap_sub_bucket_rank[b][x] = (outer_hash((b << 32)^x))%nprocs;//rcount % nprocs;

            if (gmap_sub_bucket_rank[b][x] == rank)
                gmap_bucket[b] = 1;

            distinct_g_ranks.insert(gmap_sub_bucket_rank[b][x]);
            rcount++;
        }
        gmap_distinct_sub_bucket_rank[b] = new int[distinct_g_ranks.size()];
        u32 x = 0;
        for ( auto it = distinct_g_ranks.begin(); it != distinct_g_ranks.end(); ++it )
        {
            gmap_distinct_sub_bucket_rank[b][x] = *it;
            x++;
        }
        gmap_distinct_sub_bucket_rank_count[b] = x;
    }


    int** tmap_distinct_sub_bucket_rank = new int*[buckets];
    int* tmap_distinct_sub_bucket_rank_count = new int[buckets];

    u32 *tmap_bucket = new u32[buckets];
    memset(tmap_bucket, 0, sizeof(u32) * buckets);

    rcount = 0;
    for (u64 b = 0; b < buckets; b++)
    {
        std::unordered_set<int> distinct_t_ranks;
        for (u64 x = 0; x < subbuckets_T[b]; x++)
        {
            tmap_sub_bucket_rank[b][x] = (outer_hash((b << 32)^x))%nprocs;//rcount % nprocs;

            if (tmap_sub_bucket_rank[b][x] == rank)
                tmap_bucket[b] = 1;

            distinct_t_ranks.insert(tmap_sub_bucket_rank[b][x]);
            rcount++;
        }
        tmap_distinct_sub_bucket_rank[b] = new int[distinct_t_ranks.size()];
        u32 x = 0;
        for ( auto it = distinct_t_ranks.begin(); it != distinct_t_ranks.end(); ++it )
        {
            tmap_distinct_sub_bucket_rank[b][x] = *it;
            x++;
        }
        tmap_distinct_sub_bucket_rank_count[b] = x;
    }


    u32 entry_count;
    u64 *input_buffer = NULL;
    parallel_read_input_relation_from_file_to_local_buffer(argv[1], &input_buffer, &entry_count);

    double ior_end = MPI_Wtime();

    double hash_start = MPI_Wtime();

    u32 G_hash_entry_count;
    u64 *G_hashed_data = NULL;
    buffer_data_to_hash_buffer(entry_count, input_buffer, 0, &G_hashed_data, &G_hash_entry_count, MPI_COMM_WORLD, buckets, subbuckets_G, subbuckets_T, gmap_sub_bucket_rank);

    std::cout << "[G] Rank: " << rank << " Hashed count: " << G_hash_entry_count << " Ratio: " << (float)((float) G_hash_entry_count/global_row_count)*100 << std::endl;


    u32 T_hash_entry_count;
    u64 *T_hashed_data = NULL;
    buffer_data_to_hash_buffer(entry_count, input_buffer,  1, &T_hashed_data, &T_hash_entry_count, MPI_COMM_WORLD, buckets, subbuckets_G, subbuckets_T, tmap_sub_bucket_rank);

    std::cout << "[T] Rank: " << rank << " Hashed count: " << T_hash_entry_count << " Ratio: " << (float)((float) T_hash_entry_count/global_row_count)*100 << std::endl;

    delete[] input_buffer;
    double hash_end = MPI_Wtime();

    double relation_start = MPI_Wtime();

    // this buffer is used as a bitmap

    u32 **gmap_sub_bucket = new u32*[buckets];
    memset(gmap_sub_bucket, 0, sizeof(u32*) * buckets);

    for (u32 i = 0; i < buckets; i++)
    {
        gmap_sub_bucket[i] = new u32[subbuckets_G[i]];
        memset(gmap_sub_bucket[i], 0, sizeof(u32) * subbuckets_G[i]);
    }

    Relation1Map **G = new Relation1Map*[buckets];
    for (u32 i = 0; i < buckets; i++)
        G[i] = new Relation1Map[subbuckets_G[i]];

    for (u32 i = 0; i < COL_COUNT * G_hash_entry_count; i = i + 2)
    {
        uint64_t bucket_id = outer_hash(G_hashed_data[i]) % buckets;
        uint64_t sub_bucket_id = outer_hash(G_hashed_data[i+1]) % subbuckets_G[bucket_id];
        gmap_bucket[bucket_id] = 1;
        gmap_sub_bucket[bucket_id][sub_bucket_id]++;

        auto it = G[bucket_id][sub_bucket_id].find(G_hashed_data[i]);
        if( it != G[bucket_id][sub_bucket_id].end() ) {
            auto it2 = (it->second)->find(G_hashed_data[i + 1]);
            if( it2 != (it->second)->end() ) {
                ;
            }
            else{
                (it->second)->insert(std::make_pair(G_hashed_data[i + 1], 0));
                G[bucket_id][sub_bucket_id][G_hashed_data[i]] = it->second;
            }
        }
        else {
            Relation0Map *k = new Relation0Map;
            k->insert(std::make_pair(G_hashed_data[i + 1], 0));
            G[bucket_id][sub_bucket_id].insert(std::make_pair(G_hashed_data[i],k));
        }
    }


#if 0
    for (u32 i = 0; i < buckets; i++)
    {
        for (u32 j = 0; j < subbuckets_G[i]; j++)
        {
            int counter = 0;
            for (auto it = G[i][j].begin(); it != G[i][j].end(); it++)
            {
                Relation0Map* it2 = it->second;
                for (auto dit2 = it2->begin(); dit2 != it2->end(); dit2++)
                {
                    //std::cout << "Rank " << rank << " G [" << i << ", " << j << ", " << counter << "] " << it->first << " " << dit2->first << std::endl;
                    counter++;
                }
            }
        }
    }
#endif

    //u32 tmap[buckets];
    Relation1Map **T = new Relation1Map*[buckets];
    for (u32 i = 0; i < buckets; i++)
        T[i] = new Relation1Map[subbuckets_T[i]];

    u32 **tmap_sub_bucket = new u32*[buckets];
    memset(tmap_sub_bucket, 0, sizeof(u32*) * buckets);
    for (u32 i = 0; i < buckets; i++)
    {
        tmap_sub_bucket[i] = new u32[subbuckets_T[i]];
        memset(tmap_sub_bucket[i], 0, sizeof(u32) * subbuckets_T[i]);
    }

    for (u32 i = 0; i < COL_COUNT * T_hash_entry_count; i = i + 2)
    {
        uint64_t bucket_id = outer_hash(T_hashed_data[i+1]) % buckets;
        uint64_t sub_bucket_id = outer_hash(T_hashed_data[i]) % subbuckets_T[bucket_id];
        tmap_sub_bucket[bucket_id][sub_bucket_id]++;
        tmap_bucket[bucket_id] = 1;

        auto it = T[bucket_id][sub_bucket_id].find(T_hashed_data[i]);
        if( it != T[bucket_id][sub_bucket_id].end() ) {
            auto it2 = (it->second)->find(T_hashed_data[i + 1]);
            if( it2 != (it->second)->end() ) {
                ;
            }
            else{
                (it->second)->insert(std::make_pair(T_hashed_data[i + 1], 0));
                T[bucket_id][sub_bucket_id][T_hashed_data[i]] = it->second;
            }
        }
        else {
            Relation0Map *k = new Relation0Map;
            k->insert(std::make_pair(T_hashed_data[i + 1], 0));
            T[bucket_id][sub_bucket_id].insert(std::make_pair(T_hashed_data[i],k));
        }
    }

#if 0
    for (u32 i = 0; i < buckets; i++)
    {
        for (u32 j = 0; j < subbuckets_T[i]; j++)
        {
            int counter = 0;
            for (auto it = T[i][j].begin(); it != T[i][j].end(); it++)
            {
                Relation0Map* it2 = it->second;
                for (auto dit2 = it2->begin(); dit2 != it2->end(); dit2++)
                {

                    //std::cout << "Rank " << rank << " T [" << i << ", " << j << ", " << counter << "] " << it->first << " " << dit2->first << std::endl;
                    counter++;
                }
            }
        }
    }
#endif


    u32 *dtmap_bucket = new u32[buckets];
    memset(dtmap_bucket, 0, sizeof(u32) * buckets);

    Relation1Map ***dT = new Relation1Map**[buckets];
    for (u32 i = 0; i < buckets; i++)
    {
        dT[i] = new Relation1Map*[subbuckets_T[i]];
        for (u32 j = 0; j < subbuckets_T[i]; j++)
            dT[i][j] = new Relation1Map;
    }

    u32 **dtmap_sub_bucket = new u32*[buckets];
    memset(dtmap_sub_bucket, 0, sizeof(u32*) * buckets);
    for (u32 i = 0; i < buckets; i++)
    {
        dtmap_sub_bucket[i] = new u32[subbuckets_T[i]];
        memset(dtmap_sub_bucket[i], 0, sizeof(u32) * subbuckets_T[i]);
    }

    int running_t_count = 0;
    for (u32 i = 0; i < COL_COUNT * T_hash_entry_count; i = i + 2)
    {
        uint64_t bucket_id = outer_hash(T_hashed_data[i+1]) % buckets;
        uint64_t sub_bucket_id = outer_hash(T_hashed_data[i]) % subbuckets_T[bucket_id];
        dtmap_bucket[bucket_id] = 1;
        dtmap_sub_bucket[bucket_id][sub_bucket_id]++;

        auto it = dT[bucket_id][sub_bucket_id]->find(T_hashed_data[i]);
        if( it != dT[bucket_id][sub_bucket_id]->end() ) {
            auto it2 = (it->second)->find(T_hashed_data[i + 1]);
            if( it2 != (it->second)->end() ) {
                ;
            }
            else{
                (it->second)->insert(std::make_pair(T_hashed_data[i + 1], 0));
                dT[bucket_id][sub_bucket_id]->insert(std::make_pair(T_hashed_data[i], it->second));
                running_t_count++;
            }
        }
        else {
            Relation0Map *k = new Relation0Map;
            k->insert(std::make_pair(T_hashed_data[i + 1], 0));
            dT[bucket_id][sub_bucket_id]->insert(std::make_pair(T_hashed_data[i],k));
            running_t_count++;
        }
    }


#if 1
    delete[] T_hashed_data;
    delete[] G_hashed_data;
    double relation_end = MPI_Wtime();



    double load_balance_G_start = MPI_Wtime();
    load_balance_G(buckets, gmap_bucket, gmap_sub_bucket, subbuckets_G, G, gmap_sub_bucket_rank, gmap_distinct_sub_bucket_rank_count, gmap_distinct_sub_bucket_rank);
    double load_balance_G_end = MPI_Wtime();

    double load_balance_T_start = MPI_Wtime();
    load_balance_T(buckets, tmap_bucket, tmap_sub_bucket, subbuckets_T, T, dtmap_bucket, dtmap_sub_bucket, dT, tmap_sub_bucket_rank, tmap_distinct_sub_bucket_rank_count, tmap_distinct_sub_bucket_rank);
    double load_balance_T_end = MPI_Wtime();


    u32 g_bucket_count = 0;
    for (u32 bk = 0; bk < buckets; bk++)
    {
        if (gmap_bucket[bk] == 1)
            g_bucket_count++;
    }
    u32* g_bucket_indices = new u32[g_bucket_count];

    u32 g_count = 0;
    for (u32 bk = 0; bk < buckets; bk++)
    {
        if (gmap_bucket[bk] == 1)
        {
            g_bucket_indices[g_count] = bk;
            g_count++;
        }
    }

    u32 g_t_bucket_count = 0;
    for (u32 bk = 0; bk < buckets; bk++)
    {
        if (gmap_bucket[bk] == 1 || tmap_bucket[bk] == 1)
            g_t_bucket_count++;
    }
    u32* g_t_bucket_indices = new u32[g_t_bucket_count];

    u32 g_t_count = 0;
    for (u32 bk = 0; bk < buckets; bk++)
    {
        if (gmap_bucket[bk] == 1 || tmap_bucket[bk] == 1)
        {
            g_t_bucket_indices[g_t_count] = bk;
            g_t_count++;
        }
    }

    u32 *global_gmap = new u32[buckets * nprocs];
    memset(global_gmap, 0, sizeof(u32) * buckets * nprocs);
    MPI_Allgather(gmap_bucket, buckets, MPI_INT, global_gmap, buckets, MPI_INT, MPI_COMM_WORLD);

    u32* g_recv_count = new u32[buckets];
    memset(g_recv_count, 0, buckets * sizeof(u32));

    for (u32 b = 0; b < buckets; b++)
    {
        for (u32 h = b; h < buckets * nprocs; h=h+buckets)
        {
            if (global_gmap[h] == 1)
                g_recv_count[b]++;
        }
    }

    rcount = 0;
    u32** g_recv_rank = new u32*[buckets];
    memset(g_recv_rank, 0, buckets * sizeof(u32*));
    for (u32 b = 0; b < buckets; b++)
    {
        g_recv_rank[b] = new u32[g_recv_count[b]];
        memset(g_recv_rank[b], 0, g_recv_count[b] * sizeof(u32));
    }

    for (u32 b = 0; b < buckets; b++)
    {
        rcount = 0;
        for (u32 h = b; h < buckets * nprocs; h=h+buckets)
        {
            if (global_gmap[h] == 1)
            {
                g_recv_rank[b][rcount] = h / buckets;
                rcount++;
            }
        }
    }
#if 0
    for (u32 b = 0; b < buckets; b++)
        for (u32 k = 0; k < g_recv_count[b]; k++)
            std::cout << b << "  " << k << " " << g_recv_rank[b][k] << std::endl;
#endif
    double join_start = MPI_Wtime();


#if 0
    tuple<2> dt;
    std::vector<tuple<2>> delT_temp[buckets];

    if (rank == 6)
        for (u32 i = 0; i < buckets; i++)
        {
            //std::cout << "SSSS [" << i << "] " << subbuckets_G[i] << " " << subbuckets_T[i] << std::endl;
            //std::cout << "TTTT [" << i << "] " << gmap_distinct_sub_bucket_rank_count[i] << " " << tmap_distinct_sub_bucket_rank_count[i] << std::endl;
        }

    int count_r = 0;
    int count_s = 0;
    for (u32 i3 = 0; i3 < g_t_bucket_count; i3++)
    {
        u32 i = g_t_bucket_indices[i3];
        //if (rank == 6)
        //std::cout << i << " MAP: " << tmap_bucket[i] << " " << gmap_bucket[i] << std::endl;
        if (tmap_bucket[i] == 1 || gmap_bucket[i] == 1)
        {
            for (u32 j = 0; j < subbuckets_T[i]; j++)
            {
                for (auto it = dT[i][j]->begin(); it != dT[i][j]->end(); it++)
                {
                    Relation0Map* it2 = it->second;
                    for (auto dit2 = it2->begin(); dit2 != it2->end(); dit2++)
                    {
                        dt[0] = it->first;
                        dt[1] = dit2->first;
                        delT_temp[i].push_back(dt);
                    }
                }
            }

            //int buffer_size = delT_temp[i].size() * COL_COUNT;
            //std::cout << "buffer size " << buffer_size << std::endl;
            //u32* size_buf = new u32[tmap_distinct_sub_bucket_rank_count[i]];
            //memset(size_buf, 0, sizeof(u32) * tmap_distinct_sub_bucket_rank_count[i]);


            // meta-data
            if (tmap_bucket[i] == 1)
            {
                for (int r = 0; r < gmap_distinct_sub_bucket_rank_count[i]; r++)
                {
                    count_s++;
                    //std::cout << "[X] Sending " << buffer_size << " to " << gmap_distinct_sub_bucket_rank[i][r] << std::endl;
                    //MPI_Isend(&buffer_size, 1, MPI_INT, gmap_distinct_sub_bucket_rank[i][r], 123, MPI_COMM_WORLD, &req1[req_counter1]);
                    //req_counter1++;
                }
            }

            if (gmap_bucket[i] == 1)
            {
                for (int r = 0; r < tmap_distinct_sub_bucket_rank_count[i]; r++)
                {
                    count_r++;
                    //std::cout << "[Y] Recieving " << "from " << tmap_distinct_sub_bucket_rank[i][r] << std::endl;
                    //MPI_Irecv(size_buf + r, 1, MPI_INT, tmap_distinct_sub_bucket_rank[i][r], 123, MPI_COMM_WORLD, &req1[req_counter1]);
                    //req_counter1++;
                }
            }

        }
    }
    int g_count_s = 0, g_count_r = 0;



    MPI_Allreduce(&count_r, &g_count_r, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&count_s, &g_count_s, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    std::cout << "ZZZZZZZZZZZZZZZZZZZZZ Rank " << rank << " " << g_count_s << " " << g_count_r << std::endl;
    assert(g_count_r == g_count_s);
#endif



    int lb = 0;
    double time = 0;

    dT = parallel_map_join(dT, dtmap_bucket, G, gmap_bucket, T, tmap_bucket, 0, &lb, &running_t_count, &time, buckets, subbuckets_G, subbuckets_T, gmap_sub_bucket_rank, tmap_sub_bucket_rank, g_t_bucket_indices, g_t_bucket_count, g_bucket_indices, g_bucket_count, g_recv_count, g_recv_rank, atoi(argv[4]), gmap_distinct_sub_bucket_rank_count, gmap_distinct_sub_bucket_rank, tmap_distinct_sub_bucket_rank_count, tmap_distinct_sub_bucket_rank);



#if 1
    int lc = 1;
    while(true)
    {
        dT = parallel_map_join(dT, dtmap_bucket, G, gmap_bucket, T, tmap_bucket, lc, &lb, &running_t_count, &time, buckets, subbuckets_G, subbuckets_T, gmap_sub_bucket_rank, tmap_sub_bucket_rank, g_t_bucket_indices, g_t_bucket_count, g_bucket_indices, g_bucket_count, g_recv_count, g_recv_rank, atoi(argv[4]), gmap_distinct_sub_bucket_rank_count, gmap_distinct_sub_bucket_rank, tmap_distinct_sub_bucket_rank_count, tmap_distinct_sub_bucket_rank);

        //load_balance_G(buckets, gmap_bucket, gmap_sub_bucket, subbuckets_G, G);
        //load_balance_T(buckets, tmap_bucket, tmap_sub_bucket, subbuckets_T, T, dtmap_bucket, dtmap_sub_bucket, dT);

        if (lb == 1)  break;
        lc++;
    }
#endif
    double join_end = MPI_Wtime();



    u64 total_sum = 0;
    u64 Tcounter = 0;
    for (u32 i = 0; i < buckets; i++)
    {
        for (u32 j = 0; j < subbuckets_T[i]; j++)
        {
            for ( auto local_it = T[i][j].begin(); local_it!= T[i][j].end(); ++local_it )
            {
                Relation0Map* k = local_it->second;
                for (auto it2 = k->begin(); it2 != k->end(); it2++)
                    Tcounter++;
            }
        }
    }
    MPI_Allreduce(&Tcounter, &total_sum, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, comm);



    if (rank == 0)
    {
        std::cout << "n: "
                  << nprocs
                  << " G: "
                  << global_row_count
                  << " A2a " << atoi(argv[4])
                << " T: " << total_sum
                << " Read time: " << (ior_end - ior_start)
                << " Init hash: " << (hash_end - hash_start)
                << " Relation init: " << (relation_end - relation_start)
                << " Load balG: " << (load_balance_G_end - load_balance_G_start)
                << " Load bal T: " << (load_balance_T_end - load_balance_T_start)
                << " Fixed point: " << (join_end - join_start)
                << " Total: " << (join_end - ior_start)
                << " [" << (ior_end - ior_start) + (hash_end - hash_start) + (relation_end - relation_start) + (load_balance_G_end - load_balance_G_start) + (load_balance_T_end - load_balance_T_start) + (join_end - join_start) << "]" << std::endl;
    }

    if (rank == 0)
    {
        std::cout << "n: " << nprocs
                  << " G: " << global_row_count
                  << " T: " << total_sum
                  << " Read time: " << (ior_end - ior_start)
                  << " Init hash: " << (hash_end - hash_start)
                  << " Relation init: " << (relation_end - relation_start)
                  << " Fixed point: " << (join_end - join_start)
                  << " Total: " << (join_end - ior_start)
                  << " [" << (ior_end - ior_start) + (hash_end - hash_start) + (relation_end - relation_start) + (load_balance_G_end - load_balance_G_start) + (load_balance_T_end - load_balance_T_start) + (join_end - join_start) << "]" << std::endl;
    }

    for (u32 i = 0; i < buckets; i++)
    {
        for (u32 j = 0; j < subbuckets_T[i]; j++)
        {
            Relation1Map::iterator iy1 = T[i][j].begin();
            for(; iy1 != T[i][j].end(); iy1++)
                delete (iy1->second);
        }

        for (u32 j = 0; j < subbuckets_G[i]; j++)
        {
            Relation1Map::iterator iy2 = G[i][j].begin();
            for(; iy2 != G[i][j].end(); iy2++)
                delete (iy2->second);
        }



        for (u32 j = 0; j < subbuckets_T[i]; j++)
        {
            Relation1Map::iterator iy = dT[i][j]->begin();
            for(; iy != dT[i][j]->end(); iy++)
                delete (iy->second);
            delete dT[i][j];
        }

        delete[] T[i];
        delete[] G[i];
        delete[] dT[i];

        delete[] tmap_sub_bucket_rank[i];
        delete[] gmap_sub_bucket_rank[i];

        delete[] gmap_sub_bucket[i];
        delete[] tmap_sub_bucket[i];
        delete[] dtmap_sub_bucket[i];

    }

    delete[] gmap_distinct_sub_bucket_rank_count;
    for (u64 b = 0; b < buckets; b++)
        delete[] gmap_distinct_sub_bucket_rank[b];
    delete[] gmap_distinct_sub_bucket_rank;

    delete[] tmap_distinct_sub_bucket_rank_count;
    for (u64 b = 0; b < buckets; b++)
        delete[] tmap_distinct_sub_bucket_rank[b];
    delete[] tmap_distinct_sub_bucket_rank;



    delete[] g_recv_count;
    for (u32 b = 0; b < buckets; b++)
        delete[] g_recv_rank[b];
    delete[] g_recv_rank;

    delete[] global_gmap;


    delete[] g_bucket_indices;
    delete[] g_t_bucket_indices;

    delete[] tmap_sub_bucket_rank;
    delete[] gmap_sub_bucket_rank;

    delete[] gmap_sub_bucket;
    delete[] tmap_sub_bucket;
    delete[] dtmap_sub_bucket;

    delete[] dT;
    delete[] T;
    delete[] G;

    delete[] tmap_bucket;
    delete[] gmap_bucket;
    delete[] dtmap_bucket;


    delete[] subbuckets_G;
    delete[] subbuckets_T;


#endif
#endif
    // Finalizing MPI
    MPI_Finalize();

    return 0;
}
