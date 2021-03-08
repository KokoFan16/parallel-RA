﻿#include "../src/parallel_RA_inc.h"
#include <time.h>
#include <algorithm>
#include <iterator>
#include <bitset>

#define ITERATION_COUNT 10

static void uniform_benchmark(int ra_count, int nprocs, int epoch_count, u64 entry_count);
static void non_uniform_benchmark(int ra_count, int nprocs, u64 entry_count, int random_offset, int range);
static void all_to_allv_test(all_to_allv_buffer non_uniform_buffer, MPI_Comm comm, double* all_to_all_time, double* buffer_create_time, double* buffer_delete_time, double *all_to_allv_time, int it, int nprocs, u64 entry_count, int random_offset, int range);

static void complete_uniform_benchmark(int ra_count, int nprocs, u64 entry_count, int random_offset, int range);
static void bruck_uniform_benchmark(int ra_count, int nprocs, u64 entry_count);
static void three_phases_uniform_benchmark(int ra_count, int nprocs, u64 entry_count, int node_procs);
static void uniform_ptp_benchmark(int ra_count, int nprocs, int epoch_count, u64 entry_count);


int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <number of cores per node>\n", argv[0]);
        return -1;
    }

    mpi_comm mcomm;
    mcomm.create(argc, argv);
    srand (time(NULL));
    u32 ra_count = 1;

    int node_procs = atoi(argv[1]);

//    for (u64 entry_count=2048; entry_count <= 16384; entry_count=entry_count*2)
//        non_uniform_benchmark(ra_count, mcomm.get_nprocs(), entry_count, 90, 10);
//
//
//    MPI_Barrier(MPI_COMM_WORLD);
//    if (mcomm.get_rank() == 0)
//        std::cout << "----------------------------------------------------------------" << std::endl << std::endl;
//
//    for (u64 entry_count=2048; entry_count <= 16384; entry_count=entry_count*2)
//        non_uniform_benchmark(ra_count, mcomm.get_nprocs(), entry_count, 80, 20);
//
//
//    MPI_Barrier(MPI_COMM_WORLD);
//    if (mcomm.get_rank() == 0)
//        std::cout << "----------------------------------------------------------------" << std::endl<< std::endl;
//
//    for (u64 entry_count=2048; entry_count <= 16384; entry_count=entry_count*2)
//        non_uniform_benchmark(ra_count, mcomm.get_nprocs(), entry_count, 70, 30);
//
//
//    MPI_Barrier(MPI_COMM_WORLD);
//    if (mcomm.get_rank() == 0)
//        std::cout << "----------------------------------------------------------------" << std::endl<< std::endl;
//
//    for (u64 entry_count=2048; entry_count <= 16384; entry_count=entry_count*2)
//        non_uniform_benchmark(ra_count, mcomm.get_nprocs(), entry_count, 60, 40);
//
//
//    MPI_Barrier(MPI_COMM_WORLD);
//    if (mcomm.get_rank() == 0)
//        std::cout << "----------------------------------------------------------------" << std::endl<< std::endl;
//
//    for (u64 entry_count=2048; entry_count <= 16384; entry_count=entry_count*2)
//        non_uniform_benchmark(ra_count, mcomm.get_nprocs(), entry_count, 50, 50);
//
//
//    MPI_Barrier(MPI_COMM_WORLD);
//    if (mcomm.get_rank() == 0)
//        std::cout << "----------------------------------------------------------------" << std::endl<< std::endl;
//
//    for (u64 entry_count=2048; entry_count <= 16384; entry_count=entry_count*2)
//        non_uniform_benchmark(ra_count, mcomm.get_nprocs(), entry_count, 0, 100);
//
//    MPI_Barrier(MPI_COMM_WORLD);
//        if (mcomm.get_rank() == 0)
//            std::cout << "----------------------------------------------------------------" << std::endl<< std::endl;
//
//    for (u64 entry_count=2048; entry_count <= 16384; entry_count=entry_count*2)
//    	complete_uniform_benchmark(ra_count, mcomm.get_nprocs(), entry_count, 60, 40);
//
//
//    MPI_Barrier(MPI_COMM_WORLD);
//        if (mcomm.get_rank() == 0)
//            std::cout << "----------------------------------------------------------------" << std::endl<< std::endl;
//
//    for (u64 entry_count=2048; entry_count <= 16384; entry_count=entry_count*2)
//    	complete_uniform_benchmark(ra_count, mcomm.get_nprocs(), entry_count, 50, 50);


    MPI_Barrier(MPI_COMM_WORLD);
    if (mcomm.get_rank() == 0)
        std::cout << "----------------------------------------------------------------" << std::endl<< std::endl;

    for (u64 entry_count=8; entry_count <= 4096; entry_count=entry_count*2)
//        for (u32 epoch_count=1; epoch_count<=8; epoch_count=epoch_count*2)
    	uniform_benchmark(ra_count, mcomm.get_nprocs(), 1, entry_count);

    MPI_Barrier(MPI_COMM_WORLD);
    if (mcomm.get_rank() == 0)
        std::cout << "----------------------------------------------------------------" << std::endl<< std::endl;

    for (u64 entry_count=8; entry_count <= 4096; entry_count=entry_count*2)
    	uniform_ptp_benchmark(ra_count, mcomm.get_nprocs(), 1, entry_count);

    MPI_Barrier(MPI_COMM_WORLD);
    if (mcomm.get_rank() == 0)
        std::cout << "----------------------------------------------------------------" << std::endl<< std::endl;

    for (u64 entry_count=8; entry_count <= 4096; entry_count=entry_count*2)
    {
    	int epoch_count = (entry_count / 512 > 0)? (entry_count / 512): 1;
    	uniform_ptp_benchmark(ra_count, mcomm.get_nprocs(), epoch_count, entry_count);
    }

//    for (u64 entry_count=4; entry_count <= 64; entry_count=entry_count*2)
//        three_phases_uniform_benchmark(ra_count, mcomm.get_nprocs(), entry_count, node_procs);
//
    MPI_Barrier(MPI_COMM_WORLD);
    if (mcomm.get_rank() == 0)
        std::cout << "----------------------------------------------------------------" << std::endl<< std::endl;

    for (u64 entry_count=8; entry_count <= 4096; entry_count=entry_count*2)
    	bruck_uniform_benchmark(ra_count, mcomm.get_nprocs(), entry_count);



#if 0
    MPI_Barrier(MPI_COMM_WORLD);

    for (u64 entry_count=4096; entry_count <= 16384; entry_count=entry_count*2)
    {
        for (u32 ra_count= 1; ra_count <= 8; ra_count=ra_count*2)
        {
            if (mcomm.get_rank() == 0)
            {
                std::cout << std::endl;
                std::cout << "---- [NU] nprocs " << mcomm.get_nprocs() << " ra count " << ra_count << " Entry count " << entry_count << " ----" << std::endl;
            }
            non_uniform_benchmark(ra_count, mcomm.get_nprocs(), entry_count);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    for (u64 entry_count=4096; entry_count <= 16384; entry_count=entry_count*2)
    {
        for (u32 rel_count=1; rel_count<=8; rel_count=rel_count*2)
        {
            for (u32 epoch_count=1; epoch_count<=8; epoch_count=epoch_count*2)
            {
                if (mcomm.get_rank() == 0)
                {
                    std::cout << std::endl;
                    std::cout << "[U] nprocs " << mcomm.get_nprocs() << " ra count " << rel_count << " Entry count " << entry_count << " Epoch counts " << epoch_count << " ----" << std::endl;
                }
                uniform_benchmark(rel_count, mcomm.get_nprocs(), epoch_count, entry_count);
            }
        }
    }
#endif
    mcomm.destroy();
    return 0;
}

#if 1
static void uniform_benchmark(int ra_count, int nprocs, int epoch_count, u64 entry_count)
{
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    all_to_all_buffer uniform_buffer;
    uniform_buffer.nprocs = nprocs;//mcomm.get_nprocs();
    uniform_buffer.ra_count = ra_count;//atoi(argv[1]);

    uniform_buffer.local_compute_output = new u64[(uniform_buffer.ra_count * uniform_buffer.nprocs * entry_count)/epoch_count];


    u64 *cumulative_all_to_allv_buffer = new u64[(uniform_buffer.ra_count * uniform_buffer.nprocs * entry_count)/epoch_count];

    for (int it=0; it < ITERATION_COUNT; it++)
    {
        double u_iter_buffer_total=0;
        double u_iter_a2a_total=0;
        double u_iter_buffer_time[epoch_count];
        double u_iter_a2a_time[epoch_count];
        double u_start = MPI_Wtime();

        u64 global_cumulativ_send_count = 0;
        for (int e=0; e<epoch_count; e++)
        {
            double buff_pop_start = MPI_Wtime();
            for (u64 i=0; i < (uniform_buffer.ra_count * uniform_buffer.nprocs * entry_count); i=i+epoch_count)
                uniform_buffer.local_compute_output[i/epoch_count] = i / (uniform_buffer.ra_count * entry_count);
            double buff_pop_end = MPI_Wtime();

            double a2a_start = MPI_Wtime();
            MPI_Alltoall(uniform_buffer.local_compute_output, (uniform_buffer.ra_count * entry_count)/epoch_count, MPI_UNSIGNED_LONG_LONG, cumulative_all_to_allv_buffer, (uniform_buffer.ra_count * entry_count)/epoch_count, MPI_UNSIGNED_LONG_LONG, MPI_COMM_WORLD);
            double a2a_end = MPI_Wtime();

            if (it == 0)
            {
                u64 global_send_count = 0;
                u64 send_count = ((uniform_buffer.ra_count * entry_count)/epoch_count) * nprocs;
                MPI_Allreduce(&send_count, &global_send_count, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
                global_cumulativ_send_count = global_cumulativ_send_count + global_send_count;

                if (e == epoch_count - 1)
                {
                    if (rank == 0)
                        std::cout << "[Uniform] Send and Recieve count " << nprocs << " " << epoch_count << " " << entry_count << " " << global_cumulativ_send_count << std::endl;
                    if (global_cumulativ_send_count != entry_count * nprocs * ra_count * nprocs)
                        MPI_Abort(MPI_COMM_WORLD, -1);
                }
            }

            u_iter_buffer_time[e] = buff_pop_end-buff_pop_start;
            u_iter_a2a_time[e] = a2a_end-a2a_start;

            u_iter_buffer_total = u_iter_buffer_total + u_iter_buffer_time[e];
            u_iter_a2a_total = u_iter_a2a_total + u_iter_a2a_time[e];
        }
        double u_end = MPI_Wtime();

        double max_u_time = 0;
        double total_u_time = u_end - u_start;
        MPI_Allreduce(&total_u_time, &max_u_time, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        if (total_u_time == max_u_time)
        {
            if (it == 0)
            {
                std::cout << "SKIP [U] " << it << ", " << nprocs << " [ " << entry_count << " " << epoch_count << " ] U time " << (u_end - u_start) << " [" << u_iter_buffer_total + u_iter_a2a_total << "] " << u_iter_buffer_total << " ( ";
            for (int i=0; i<epoch_count; i++)
                std::cout << u_iter_buffer_time[i] << " ";
            std::cout << ") " << u_iter_a2a_total << " ( ";
            for (int i=0; i<epoch_count; i++)
                std::cout << u_iter_a2a_time[i] << " ";
            std::cout << ")" << std::endl;
            }
            else
            {
                std::cout << "[U] " << it << ", " << nprocs << " [ " << entry_count << " " << epoch_count << " ] U time " << (u_end - u_start) << " [" << u_iter_buffer_total + u_iter_a2a_total << "] " << u_iter_buffer_total << " ( ";
            for (int i=0; i<epoch_count; i++)
                std::cout << u_iter_buffer_time[i] << " ";
            std::cout << ") " << u_iter_a2a_total << " ( ";
            for (int i=0; i<epoch_count; i++)
                std::cout << u_iter_a2a_time[i] << " ";
            std::cout << ")" << std::endl;

            }
        }


    }

    delete[] cumulative_all_to_allv_buffer;
    delete[] uniform_buffer.local_compute_output;
}
#endif



static void uniform_ptp_benchmark(int ra_count, int nprocs, int epoch_count, u64 entry_count)
{
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    all_to_all_buffer uniform_buffer;
    uniform_buffer.nprocs = nprocs;//mcomm.get_nprocs();
    uniform_buffer.ra_count = ra_count;//atoi(argv[1]);

    uniform_buffer.local_compute_output = new u64[(uniform_buffer.ra_count * uniform_buffer.nprocs * entry_count)/epoch_count];


    u64 *cumulative_all_to_allv_buffer = new u64[(uniform_buffer.ra_count * uniform_buffer.nprocs * entry_count)/epoch_count];

    for (int it=0; it < ITERATION_COUNT; it++)
    {
        double u_iter_buffer_total=0;
        double u_iter_a2a_total=0;
        double u_iter_buffer_time[epoch_count];
        double u_iter_a2a_time[epoch_count];
        double u_start = MPI_Wtime();

        u64 global_cumulativ_send_count = 0;
        for (int e=0; e<epoch_count; e++)
        {
            double buff_pop_start = MPI_Wtime();
            for (u64 i=0; i < (uniform_buffer.ra_count * uniform_buffer.nprocs * entry_count); i=i+epoch_count)
                uniform_buffer.local_compute_output[i/epoch_count] = i / (uniform_buffer.ra_count * entry_count);
            double buff_pop_end = MPI_Wtime();

            double a2a_start = MPI_Wtime();

            int req_count = 0;
			MPI_Request req[2*nprocs];
			MPI_Status stat[2*nprocs];
			u64 exchange_count = ra_count*entry_count/epoch_count;
			for (int i = 0; i < nprocs; i++)
			{
				int comm_p = (rank + i) % nprocs; // avoid always to reach first master node
				MPI_Irecv(&uniform_buffer.local_compute_output[comm_p*exchange_count], exchange_count, MPI_UNSIGNED_LONG_LONG, comm_p, comm_p, MPI_COMM_WORLD, &req[req_count]);
				req_count++;

				MPI_Isend(&cumulative_all_to_allv_buffer[comm_p*exchange_count], exchange_count, MPI_UNSIGNED_LONG_LONG, comm_p, rank, MPI_COMM_WORLD, &req[req_count]);
				req_count++;
			}
			MPI_Waitall(req_count, req, stat);

            double a2a_end = MPI_Wtime();

            if (it == 0)
            {
                u64 global_send_count = 0;
                u64 send_count = ((uniform_buffer.ra_count * entry_count)/epoch_count) * nprocs;
                MPI_Allreduce(&send_count, &global_send_count, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
                global_cumulativ_send_count = global_cumulativ_send_count + global_send_count;

                if (e == epoch_count - 1)
                {
                    if (rank == 0)
                        std::cout << "[Uniform] Send and Recieve count " << nprocs << " " << epoch_count << " " << entry_count << " " << global_cumulativ_send_count << std::endl;
                    if (global_cumulativ_send_count != entry_count * nprocs * ra_count * nprocs)
                        MPI_Abort(MPI_COMM_WORLD, -1);
                }
            }

            u_iter_buffer_time[e] = buff_pop_end-buff_pop_start;
            u_iter_a2a_time[e] = a2a_end-a2a_start;

            u_iter_buffer_total = u_iter_buffer_total + u_iter_buffer_time[e];
            u_iter_a2a_total = u_iter_a2a_total + u_iter_a2a_time[e];
        }
        double u_end = MPI_Wtime();

        double max_u_time = 0;
        double total_u_time = u_end - u_start;
        MPI_Allreduce(&total_u_time, &max_u_time, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        if (total_u_time == max_u_time)
        {
            if (it == 0)
            {
                std::cout << "SKIP [PU] " << it << ", " << nprocs << " [ " << entry_count << " " << epoch_count << " ] PU time " << (u_end - u_start) << " [" << u_iter_buffer_total + u_iter_a2a_total << "] " << u_iter_buffer_total << " ( ";
            for (int i=0; i<epoch_count; i++)
                std::cout << u_iter_buffer_time[i] << " ";
            std::cout << ") " << u_iter_a2a_total << " ( ";
            for (int i=0; i<epoch_count; i++)
                std::cout << u_iter_a2a_time[i] << " ";
            std::cout << ")" << std::endl;
            }
            else
            {
                std::cout << "[PU] " << it << ", " << nprocs << " [ " << entry_count << " " << epoch_count << " ] PU time " << (u_end - u_start) << " [" << u_iter_buffer_total + u_iter_a2a_total << "] " << u_iter_buffer_total << " ( ";
            for (int i=0; i<epoch_count; i++)
                std::cout << u_iter_buffer_time[i] << " ";
            std::cout << ") " << u_iter_a2a_total << " ( ";
            for (int i=0; i<epoch_count; i++)
                std::cout << u_iter_a2a_time[i] << " ";
            std::cout << ")" << std::endl;

            }
        }
    }

    delete[] cumulative_all_to_allv_buffer;
    delete[] uniform_buffer.local_compute_output;
}


static void three_phases_uniform_benchmark(int ra_count, int nprocs, u64 entry_count, int node_procs)
{
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int node_id = rank / node_procs;
    int master_rank = node_id * node_procs; // 0 * 64, 1 * 64

    u64 unit_count = ra_count * entry_count;
    u64 local_count = unit_count * nprocs;
    std::vector<u64> local_compute_output(local_count);

    std::vector<u64> node_all_to_all_buffer;
    std::vector<u64> cumulative_all_to_allv_buffer;

    for (int it=0; it < ITERATION_COUNT; it++)
    {
    	double u_start = MPI_Wtime();

        double buff_pop_start = MPI_Wtime();
        for (u64 i=0; i < local_count; i++)
            local_compute_output[i] = i / (unit_count);
        double buff_pop_end = MPI_Wtime();

        // 1. gather data from other process on the same node to one master rank on that node
        double gather_node_procs_start = MPI_Wtime();
        int comm_count = (rank == master_rank)? (node_procs + 1) : 1;
        MPI_Request req[comm_count];
        MPI_Status stat[comm_count];
        int req_count = 0;
        if (rank == master_rank)
        {
        	node_all_to_all_buffer.reserve(local_count * node_procs);
        	cumulative_all_to_allv_buffer.reserve(local_count * node_procs);

        	for (int i = 0; i < node_procs; i++)
        	{
        		int recv_rank = node_id * node_procs + i;  // 3 * 64 + (0 ~ 64)
        		MPI_Irecv(&node_all_to_all_buffer.data()[i*local_count], local_count, MPI_UNSIGNED_LONG_LONG, recv_rank, recv_rank, MPI_COMM_WORLD, &req[req_count]);
        		req_count++;
        	}
        }

        MPI_Isend(local_compute_output.data(), local_count, MPI_UNSIGNED_LONG_LONG, master_rank, rank, MPI_COMM_WORLD, &req[req_count]);
		req_count++;
		MPI_Waitall(req_count, req, stat);
		double gather_node_procs_end = MPI_Wtime();


		double reorder_time = 0, ata_time = 0, reorder_2_time = 0;
		u64 blocklength = unit_count * node_procs;
		int node_num = nprocs / node_procs;
		if (rank == master_rank)
		{
	        // 2. exchange order of buffer
			//    e.g., 0 2 [0 1 2 3 0 1 2 3] to [0 1 0 1 2 3 2 3] (each process [0 1 2 3])
			double reorder_start = MPI_Wtime();
			for (int i = 0; i < node_num; i++)
			{
				for (int j = 0; j < node_procs; j++)
				{
					int offset = j * local_count + i * blocklength;
					int index = i * node_procs + j;
					memcpy(&cumulative_all_to_allv_buffer.data()[index * blocklength], &node_all_to_all_buffer.data()[offset], blocklength * sizeof(long long));
				}
			}
			double reorder_end = MPI_Wtime();
			reorder_time = reorder_end - reorder_start;

			// 3. all to all communication across all the master ranks
			double ata_start = MPI_Wtime();
			req_count = 0;
			MPI_Request req_1[2*node_num];
			MPI_Status stat_1[2*node_num];
			u64 exchange_count = blocklength * node_procs;
			for (int i = 0; i < node_num; i++)
			{
				int comm_node_id = (node_id + i) % node_num; // avoid always to reach first master node
				int comm_p = comm_node_id * node_procs;
				MPI_Irecv(&node_all_to_all_buffer.data()[comm_node_id*exchange_count], exchange_count, MPI_UNSIGNED_LONG_LONG, comm_p, comm_p, MPI_COMM_WORLD, &req_1[req_count]);
				req_count++;

				MPI_Isend(&cumulative_all_to_allv_buffer.data()[comm_node_id*exchange_count], exchange_count, MPI_UNSIGNED_LONG_LONG, comm_p, rank, MPI_COMM_WORLD, &req_1[req_count]);
				req_count++;
			}
			MPI_Waitall(req_count, req_1, stat_1);
			double ata_end = MPI_Wtime();
			ata_time = ata_end - ata_start;


			// 4. reorder [0 1 0 1 0 1 0 1] --> [0 0 0 0 1 1 1 1]
			double reorder_2_start = MPI_Wtime();
			for (int i = 0; i < node_procs; i++)
			{
				for (int j = 0; j < nprocs; j++)
				{
					int offset = i * unit_count + j * blocklength;
					int index = i * nprocs + j;
					memcpy(&cumulative_all_to_allv_buffer.data()[index * unit_count], &node_all_to_all_buffer.data()[offset], unit_count * sizeof(long long));
				}
			}
			double reorder_2_end = MPI_Wtime();
			reorder_2_time = reorder_2_end - reorder_2_start;
		}

		// 5. master rank assign data to each process on the same node
		double scatter_start = MPI_Wtime();
		req_count = 0;
		MPI_Irecv(local_compute_output.data(), local_count, MPI_UNSIGNED_LONG_LONG, master_rank, rank, MPI_COMM_WORLD, &req[req_count]);
		req_count++;

        if (rank == master_rank)
        {
        	for (int i = 0; i < node_procs; i++)
        	{
        		int send_rank = node_id * node_procs + i;
        		MPI_Isend(&cumulative_all_to_allv_buffer.data()[i*local_count], local_count, MPI_UNSIGNED_LONG_LONG, send_rank, send_rank, MPI_COMM_WORLD, &req[req_count]);
        		req_count++;
        	}
        }
        MPI_Waitall(req_count, req, stat);
        double scatter_end = MPI_Wtime();

        double u_end = MPI_Wtime();

        double max_u_time = 0;
        double total_u_time = u_end - u_start;
        MPI_Allreduce(&total_u_time, &max_u_time, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);

        double buff_pop_time = buff_pop_end - buff_pop_start;
        double gather_time = gather_node_procs_end - gather_node_procs_start;
        double scatter_time = scatter_end - scatter_start;


        if (total_u_time == max_u_time)
		{
			if (it == 0)
			{
				std::cout << "SKIP [TPU] " << it << ", " << nprocs << " [ " << entry_count << " ] TPU time " << (u_end - u_start) << " [ "
						<< buff_pop_time << " " << gather_time << " " << reorder_time << " " << ata_time << " " << reorder_2_time << " " << scatter_time << " ] " << std::endl;
			}
			else
			{
				std::cout << "[TPU] " << it << ", " << nprocs << " [ " << entry_count << " ] TPU time " << (u_end - u_start) << " [ "
						<< buff_pop_time << " " << gather_time << " " << reorder_time << " " << ata_time << " " << reorder_2_time << " " << scatter_time << " ] " << std::endl;
			}
		}
    }

    std::vector<u64>().swap(local_compute_output);
    std::vector<u64>().swap(node_all_to_all_buffer);
    std::vector<u64>().swap(cumulative_all_to_allv_buffer);
    local_compute_output.clear();
    node_all_to_all_buffer.clear();
    cumulative_all_to_allv_buffer.clear();
}


static void bruck_uniform_benchmark(int ra_count, int nprocs, u64 entry_count)
{
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    u64 unit_count = ra_count * entry_count;
    u64 local_count = ra_count * nprocs * entry_count;

    std::vector<u64> local_compute_output(local_count);
    std::vector<u64> cumulative_all_to_allv_buffer(local_count);
    std::vector<u64> temp_all_to_allv_buffer(local_count);

    for (int it=0; it < ITERATION_COUNT; it++)
    {
    	double u_start = MPI_Wtime();

        double buff_pop_start = MPI_Wtime();
        for (u64 i=0; i < (ra_count * nprocs * entry_count); i++)
            local_compute_output[i] = i / (ra_count * entry_count);
        double buff_pop_end = MPI_Wtime();

		// 1. local rotation
        double rotation_start = MPI_Wtime();
        u64 shift_count = rank * unit_count;
        std::rotate(local_compute_output.begin(), local_compute_output.begin()+shift_count, local_compute_output.end());
        double rotation_end = MPI_Wtime();

        int step = ceil(log2(nprocs));
        double ata_start = MPI_Wtime();
        double find_blocks_times[step];
        double copy_times[step];
        double comm_times[step];
        double replace_times[step];
        double total_find_blocks_time = 0, total_copy_time = 0, total_comm_time = 0, total_replace_time = 0;
        for (int k = 0; k < step; k++)
        {
            // 2. find which data blocks to send
        	double find_blocks_start = MPI_Wtime();
        	int recv_proc = rank - pow(2.0, k); // receive data from rank - 2^k process
        	int send_proc = rank + pow(2.0, k); // send data from rank + 2^k process

        	if (recv_proc < 0) recv_proc += nprocs; // boundary
        	if (send_proc > nprocs - 1) send_proc -= nprocs; // boundary

        	std::vector<int> send_indexes; // send all those data blocks whose kth (right to left) bit is 1
        	for (int i = 0; i < nprocs; i++)
        	{
        		std::string binary_rank = std::bitset<8>(i).to_string(); // convert to binary
        		int k_bit = (binary_rank.length() - k - 1); // kth bit
        		if (binary_rank[k_bit] == '1') // kth (right to left) bit is 1
        			send_indexes.push_back(i);
        	}
        	double find_blocks_end = MPI_Wtime();
        	double find_blocks_time = find_blocks_end - find_blocks_start;
        	find_blocks_times[k] = find_blocks_time;
        	total_find_blocks_time += find_blocks_time;

        	// 3. copy blocks which need to be sent at this step
        	double copy_start = MPI_Wtime();
        	int send_N = send_indexes.size();
        	for (int i = 0; i < send_N; i++)
        	{
        		u64 offset = send_indexes[i] * unit_count;
        		memcpy(&temp_all_to_allv_buffer.data()[i*unit_count], &local_compute_output.data()[offset], unit_count*sizeof(long long));
        	}
        	double copy_end = MPI_Wtime();
        	double copy_time = copy_end - copy_start;
        	copy_times[k] = copy_time;
        	total_copy_time += copy_time;

        	// 4. send and receive
        	double comm_start = MPI_Wtime();
            MPI_Request req[2];
            MPI_Status stat[2];
            u64 comm_count_per_step = send_N * unit_count;
        	MPI_Irecv(cumulative_all_to_allv_buffer.data(), comm_count_per_step, MPI_UNSIGNED_LONG_LONG, recv_proc, recv_proc, MPI_COMM_WORLD, &req[0]);
        	MPI_Isend(temp_all_to_allv_buffer.data(), comm_count_per_step, MPI_UNSIGNED_LONG_LONG, send_proc, rank, MPI_COMM_WORLD, &req[1]);
        	MPI_Waitall(2, req, stat);
        	double comm_end = MPI_Wtime();
        	double comm_time = comm_end - comm_start;
        	comm_times[k] = comm_time;
        	total_comm_time += comm_time;

        	// 5. replace with received data
        	double replace_start = MPI_Wtime();
        	for (int i = 0; i < send_N; i++)
        	{
        		u64 offset = send_indexes[i] * unit_count;
        		memcpy(&local_compute_output.data()[offset], &cumulative_all_to_allv_buffer.data()[i*unit_count], unit_count*sizeof(long long));
        	}
        	double replace_end = MPI_Wtime();
        	double replace_time = replace_end - replace_start;
        	replace_times[k] = replace_time;
        	total_replace_time += replace_time;
        }
        double ata_end = MPI_Wtime();

        // 6. local inverse rotation
        double rotation_2_start = MPI_Wtime();
        std::rotate(local_compute_output.begin(), local_compute_output.begin()+shift_count, local_compute_output.end());
        double rotation_2_end = MPI_Wtime();

        double u_end = MPI_Wtime();

        double buff_pop_time = buff_pop_end - buff_pop_start;
        double rotation_time = rotation_end - rotation_start;
        double ata_time = ata_end - ata_start;
        double rotation_2_time = rotation_2_end - rotation_2_start;

        double max_u_time = 0;
        double total_u_time = u_end - u_start;
        MPI_Allreduce(&total_u_time, &max_u_time, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        if (total_u_time == max_u_time)
        {
            if (it == 0)
            {
                std::cout << "SKIP [BU] " << it << ", " << nprocs << " [ " << entry_count <<  " ] BU time " << total_u_time << " " << buff_pop_time << " " << rotation_time << " "
                		<< ata_time << " [ " << total_find_blocks_time << " ( ";
                for (int i = 0; i < step; i++)
                	std::cout << find_blocks_times[i] << " ";
                std::cout << ") " << total_copy_time << " ( ";
                for (int i = 0; i < step; i++)
                	std::cout << copy_times[i] << " ";
                std::cout << ") " << total_comm_time << " ( ";
                for (int i = 0; i < step; i++)
                	std::cout << comm_times[i] << " ";
                std::cout << ") " << total_replace_time << " ( ";
                for (int i = 0; i < step; i++)
                	std::cout << replace_times[i] << " ";
                std::cout << ")] " << rotation_2_time << std::endl;
            }
            else
            {
                std::cout << "[BU] " << it << ", " << nprocs << " [ " << entry_count <<  " ] BU time " << total_u_time << " " << buff_pop_time << " " << rotation_time << " "
                		<< ata_time << " [ " << total_find_blocks_time << " ( ";
                for (int i = 0; i < step; i++)
                	std::cout << find_blocks_times[i] << " ";
                std::cout << ") " << total_copy_time << " ( ";
                for (int i = 0; i < step; i++)
                	std::cout << copy_times[i] << " ";
                std::cout << ") " << total_comm_time << " ( ";
                for (int i = 0; i < step; i++)
                	std::cout << comm_times[i] << " ";
                std::cout << ") " << total_replace_time << " ( ";
                for (int i = 0; i < step; i++)
                	std::cout << replace_times[i] << " ";
                std::cout << ")] " << rotation_2_time << std::endl;
            }
        }
    }

    std::vector<u64>().swap(local_compute_output);
    std::vector<u64>().swap(cumulative_all_to_allv_buffer);
    std::vector<u64>().swap(temp_all_to_allv_buffer);
    local_compute_output.clear();
    cumulative_all_to_allv_buffer.clear();
    temp_all_to_allv_buffer.clear();
}


static void non_uniform_benchmark(int ra_count, int nprocs, u64 entry_count, int random_offset, int range)
{
    all_to_allv_buffer non_uniform_buffer;
    non_uniform_buffer.nprocs = nprocs;
    non_uniform_buffer.ra_count = ra_count;
    non_uniform_buffer.local_compute_output_size_total = 0;

    non_uniform_buffer.local_compute_output_size_flat = new int[non_uniform_buffer.nprocs * non_uniform_buffer.ra_count];
    non_uniform_buffer.cumulative_tuple_process_map = new int[non_uniform_buffer.nprocs];
    memset(non_uniform_buffer.cumulative_tuple_process_map, 0, non_uniform_buffer.nprocs * sizeof(int));

    for (int r=0; r < non_uniform_buffer.ra_count; r++)
    {
        for (int i=0; i < non_uniform_buffer.nprocs; i++)
        {
        	int random = random_offset + rand() % range;
        	non_uniform_buffer.local_compute_output_size_flat[i * non_uniform_buffer.ra_count + r] = (entry_count * random) / 100;
        	non_uniform_buffer.cumulative_tuple_process_map[i] = non_uniform_buffer.cumulative_tuple_process_map[i] + non_uniform_buffer.local_compute_output_size_flat[i * non_uniform_buffer.ra_count + r];
        }
    }

    for (int it=0; it < ITERATION_COUNT; it++)
    {
        non_uniform_buffer.local_compute_output = new vector_buffer*[non_uniform_buffer.ra_count];
        for (int r=0; r < non_uniform_buffer.ra_count; r++)
            non_uniform_buffer.local_compute_output[r] = new vector_buffer[non_uniform_buffer.nprocs];

        double buffer_creation_time=0, all_to_allv_time=0, buffer_deletion_time=0;

        double nu_start = MPI_Wtime();

        double buffer_creation_start=MPI_Wtime();
        non_uniform_buffer.local_compute_output_size_total=0;
//        memset(non_uniform_buffer.cumulative_tuple_process_map, 0, non_uniform_buffer.nprocs * sizeof(int));
//        memset(non_uniform_buffer.local_compute_output_size_flat, 0, non_uniform_buffer.nprocs * non_uniform_buffer.ra_count * sizeof(int));

        for (int r=0; r < non_uniform_buffer.ra_count; r++)
        {
            for (int i=0; i < non_uniform_buffer.nprocs; i++)
            {
                u64 val = i;
                for (int t=0; t < non_uniform_buffer.local_compute_output_size_flat[i * non_uniform_buffer.ra_count + r]; t++)
                {
                    non_uniform_buffer.local_compute_output[r][i].vector_buffer_append((const unsigned char*)&val, sizeof(u64));
                    non_uniform_buffer.local_compute_output_size_total++;
                }
            }
        }

        double buffer_creation_end=MPI_Wtime();
        buffer_creation_time = buffer_creation_end - buffer_creation_start;

        double all_to_allv_start=MPI_Wtime();
        double a2a_all_to_all_time, a2a_buffer_creation_time, a2a_buffer_deletion_time, a2a_all_to_allv_time;
        all_to_allv_test(non_uniform_buffer, MPI_COMM_WORLD, &a2a_all_to_all_time, &a2a_buffer_creation_time, &a2a_buffer_deletion_time, &a2a_all_to_allv_time, it, nprocs, entry_count, random_offset, range);
        double all_to_allv_end=MPI_Wtime();
        all_to_allv_time = all_to_allv_end - all_to_allv_start;


        double buffer_deletion_start=MPI_Wtime();
        for (int r=0; r < non_uniform_buffer.ra_count; r++)
            for (int i=0; i < non_uniform_buffer.nprocs; i++)
                non_uniform_buffer.local_compute_output[r][i].vector_buffer_free();
        double buffer_deletion_end=MPI_Wtime();
        buffer_deletion_time = buffer_deletion_end-buffer_deletion_start;

        double nu_end = MPI_Wtime();

        double max_nu_time = 0;
        double total_nu_time = nu_end - nu_start;
        MPI_Allreduce(&total_nu_time, &max_nu_time, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);


        if (max_nu_time == total_nu_time){
            if (it == 0)
                std::cout << "SKIP [NU] " << it << ", " << nprocs << " [ " << random_offset << " " << range << " ] " << entry_count << " NU time " << (nu_end - nu_start) << " [" << buffer_creation_time + all_to_allv_time + buffer_deletion_time << "] Buff pop " << buffer_creation_time << " NA2A " << all_to_allv_time << " [ a2a " << a2a_all_to_all_time << " bct " << a2a_buffer_creation_time << " a2av " << a2a_all_to_allv_time << " bdt " << a2a_buffer_deletion_time << " ] Buff del " << buffer_deletion_time << " " << std::endl;
            else
                std::cout << "[NU] " << it << ", " << nprocs << " [ " << random_offset << " " << range << " ] " << entry_count << " NU time " << (nu_end - nu_start) << " [" << buffer_creation_time + all_to_allv_time + buffer_deletion_time << "] Buff pop " << buffer_creation_time << " NA2A " << all_to_allv_time << " [ a2a " << a2a_all_to_all_time << " bct " << a2a_buffer_creation_time << " a2av " << a2a_all_to_allv_time << " bdt " << a2a_buffer_deletion_time << " ] Buff del " << buffer_deletion_time << " " << std::endl;
        }

        for (int i=0; i < non_uniform_buffer.ra_count; i++)
            delete[] non_uniform_buffer.local_compute_output[i];
        delete[] non_uniform_buffer.local_compute_output;
    }


    delete[] non_uniform_buffer.local_compute_output_size_flat;
    delete[] non_uniform_buffer.cumulative_tuple_process_map;
}


#if 1
static void all_to_allv_test(all_to_allv_buffer non_uniform_buffer, MPI_Comm comm, double* all_to_all_time, double* buffer_create_time, double* buffer_delete_time, double *all_to_allv_time, int it, int nprocs, u64 entry_count, int random_offset, int range)
{
    double all_to_all_start = MPI_Wtime();
    int outer_hash_buffer_size = 0;
    u32 RA_count = non_uniform_buffer.ra_count;
    int *recv_buffer_offset_size = new int[RA_count * nprocs];
    MPI_Alltoall(non_uniform_buffer.local_compute_output_size_flat, RA_count, MPI_INT, recv_buffer_offset_size, RA_count, MPI_INT, comm);
    double all_to_all_end = MPI_Wtime();
    *all_to_all_time = all_to_all_end - all_to_all_start;

    double bt_start = MPI_Wtime();
    int *send_disp = new int[nprocs];
    int *recv_counts = new int[nprocs];
    int *recv_displacements = new int[nprocs];

    recv_counts[0] = 0;
    send_disp[0] = 0;
    recv_displacements[0] = 0;

    u64* send_buffer = new u64[non_uniform_buffer.local_compute_output_size_total];

    u32 boffset = 0;
    for(int i = 0; i < nprocs; i++)
    {
        recv_counts[i] = 0;

        if (i >= 1)
            send_disp[i] = send_disp[i - 1] + non_uniform_buffer.cumulative_tuple_process_map[i - 1];

        for (u32 r = 0; r < RA_count; r++)
        {
            memcpy(send_buffer + boffset, non_uniform_buffer.local_compute_output[r][i].buffer, non_uniform_buffer.local_compute_output[r][i].size);
            boffset = boffset + (non_uniform_buffer.local_compute_output[r][i].size)/sizeof(u64);
            recv_counts[i] = recv_counts[i] + recv_buffer_offset_size[i*RA_count + r];
        }

        if (i >= 1)
            recv_displacements[i] = recv_displacements[i - 1] + recv_counts[i - 1];
        outer_hash_buffer_size = outer_hash_buffer_size + recv_counts[i];
    }

    u64 *recv_buffer = new u64[outer_hash_buffer_size];
    double bt_end = MPI_Wtime();
    *buffer_create_time = bt_end - bt_start;

    double atv_start = MPI_Wtime();
    MPI_Alltoallv(send_buffer, non_uniform_buffer.cumulative_tuple_process_map, send_disp, MPI_UNSIGNED_LONG_LONG, recv_buffer, recv_counts, recv_displacements, MPI_UNSIGNED_LONG_LONG, comm);
    double atv_end = MPI_Wtime();
    *all_to_allv_time = atv_end - atv_start;

    if (it == 0)
    {
        int rank;
        MPI_Comm_rank(comm, &rank);
        u64 total_send_count=0;
        u64 total_recv_count=0;
        u64 global_total_send_count=0;
        u64 global_total_recv_count=0;
        for (int i=0; i < nprocs; i++)
        {
            total_send_count = total_send_count + non_uniform_buffer.cumulative_tuple_process_map[i];
            total_recv_count = total_recv_count + recv_counts[i];
        }
        MPI_Allreduce(&total_send_count, &global_total_send_count, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(&total_recv_count, &global_total_recv_count, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);

        if (rank == 0)
        {
            std::cout << "[Non Uniform] " << nprocs << " , " << entry_count << " , " << random_offset << " , "  << range << " ,  Total recv count " << global_total_recv_count  << " Total send count " << global_total_send_count << std::endl;

            std::cout << "Send count list [ ";
            for (int i=0; i < nprocs; i++)
            	std::cout << non_uniform_buffer.local_compute_output_size_flat[i] << ", ";
            std::cout << "]" << std::endl;
        }

        if (global_total_recv_count != global_total_send_count)
            MPI_Abort(MPI_COMM_WORLD, -1);
        if (global_total_recv_count != global_total_send_count)
            MPI_Abort(MPI_COMM_WORLD, -1);
    }

    double bt_d_start = MPI_Wtime();
    delete[] recv_buffer;
    delete[] send_buffer;
    delete[] send_disp;
    delete[] recv_displacements;
    delete[] recv_counts;
    delete[] recv_buffer_offset_size;
    double bt_d_end = MPI_Wtime();
    *buffer_delete_time = bt_d_end - bt_d_start;
}
#endif


static void complete_uniform_benchmark(int ra_count, int nprocs, u64 entry_count, int random_offset, int range)
{
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    all_to_allv_buffer non_uniform_buffer;
    non_uniform_buffer.local_compute_output_size_flat = new int[nprocs * ra_count];

    int max_random = 0;
    for (int r=0; r < ra_count; r++)
    {
        for (int i=0; i < nprocs; i++)
        {
        	int random = random_offset + rand() % range;
        	non_uniform_buffer.local_compute_output_size_flat[i * ra_count + r] = (entry_count * random) / 100;
        	if (((entry_count * random) / 100) > max_random)
        		max_random = (entry_count * random) / 100;
        }
    }

    int max_send_count = 0;
    MPI_Allreduce(&max_random, &max_send_count, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

	all_to_all_buffer uniform_buffer;
	uniform_buffer.local_compute_output = new u64[(ra_count * nprocs * max_send_count)];

	u64 *cumulative_all_to_allv_buffer = new u64[(ra_count * nprocs * max_send_count)];

    for (int it=0; it < ITERATION_COUNT; it++)
    {
    	double u_start = MPI_Wtime();
		double buff_pop_start = MPI_Wtime();
		for (u64 i=0; i < (ra_count * nprocs * max_send_count); i++)
			uniform_buffer.local_compute_output[i] = i / (ra_count * max_send_count);
		double buff_pop_end = MPI_Wtime();

		double a2a_start = MPI_Wtime();
		MPI_Alltoall(uniform_buffer.local_compute_output, (ra_count * max_send_count), MPI_UNSIGNED_LONG_LONG, cumulative_all_to_allv_buffer, (ra_count * max_send_count), MPI_UNSIGNED_LONG_LONG, MPI_COMM_WORLD);
		double a2a_end = MPI_Wtime();

		double u_end = MPI_Wtime();

        double u_iter_buffer_time = buff_pop_end - buff_pop_start;
        double u_iter_a2a_time = a2a_end - a2a_start;

	    double max_u_time = 0;
		double total_u_time = u_end - u_start;
		MPI_Allreduce(&total_u_time, &max_u_time, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
		if (total_u_time == max_u_time)
		{
			if (it == 0)
			{
				std::cout << "SKIP [CU] " << it << ", " << nprocs << " [ " << entry_count << " " << random_offset << " " << range <<  " ] CU time " << (u_end - u_start)
						<< " [" << u_iter_buffer_time + u_iter_a2a_time << "] Buff pop " << u_iter_buffer_time << " a2a " << u_iter_a2a_time << std::endl;
			}
			else
				std::cout << "[CU] " << it << ", " << nprocs << " [ " << entry_count << " " << random_offset << " " << range << " ] CU time " << (u_end - u_start)
						<< " [" << u_iter_buffer_time + u_iter_a2a_time << "] Buff pop " << u_iter_buffer_time  << " a2a " << u_iter_a2a_time << std::endl;
		}
    }

    u64 global_total_send_count = max_send_count * nprocs * nprocs;

    if(rank == 0)
    	std::cout << "[Complete Uniform] Max send count " << max_send_count  << ", total send count " << global_total_send_count << std::endl;

    delete[] non_uniform_buffer.local_compute_output_size_flat;
    delete[] cumulative_all_to_allv_buffer;
    delete[] uniform_buffer.local_compute_output;
}
