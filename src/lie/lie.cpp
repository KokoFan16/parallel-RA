/*
 * Logical Inferencing Engine (LIE)
 * Copyright (c) Sidharth Kumar, et al, see License.md
 */



#include "../parallel_RA_inc.h"
//#include <experimental/filesystem>


void LIE::add_relation(relation* rel)
{
    lie_relations[lie_relation_count] = rel;
    lie_relation_count++;
}

void LIE::add_scc(RAM* ra)
{
    lie_sccs[lie_sccs_count] = ra;
    lie_sccs_count++;
}


/// This function currentl only returns one runnable task
/// size of returned list is always going to be 1
RAM* LIE::one_runnable_tasks()
{
    u32 counter = 0;
    for (u32 i=0; i < lie_sccs_count; i++)
    {
        if (lie_sccs[i] == NULL)
        {
            counter++;
            continue;
        }
        if (counter == lie_sccs_count)
            return NULL;

        bool break_loop = false;
        for (auto it = taskgraph.begin(); it != taskgraph.end(); it++)
        {
            std::set<RAM*> it2 = it->second;
            for (auto dit2 = it2.begin(); dit2 != it2.end(); dit2++)
            {
                if (lie_sccs[i] == *dit2)
                {
                    break_loop=true;
                    break;
                }
            }
            if (break_loop==true)
                break;
        }
        if (break_loop==false)
            return lie_sccs[i];
    }

    return NULL;
}



void LIE::update_task_graph(RAM* executable_task)
{
    for (u32 i=0; i < lie_sccs_count; i++)
    {
        if (lie_sccs[i] == executable_task)
        {
            taskgraph.erase(lie_sccs[i]);
            delete lie_sccs[i];
            lie_sccs[i] = NULL;
        }
    }
}



void LIE::add_scc_dependance (RAM* src_task, RAM* destination_task)
{
    auto it = taskgraph.find(src_task);
    if( it != taskgraph.end() )
    {
        auto it2 = (it->second).find(destination_task);
        if( it2 == (it->second).end() )
        {
            (it->second).insert(destination_task);
            taskgraph[src_task] = it->second;
        }
    }
    else
    {
        std::set<RAM*> k;
        k.insert(destination_task);
        taskgraph.insert(std::make_pair(src_task, k));
    }

    std::set<RAM*> temp = taskgraph[src_task];
    temp.insert(destination_task);

    return;
}


#if 0
void LIE::print_all_relation()
{
    u64 total_facts=0;
    for (u32 i = 0 ; i < lie_relation_count; i++)
    {
        relation* curr_relation = lie_relations[i];
        u64 local_facts = curr_relation->get_full_element_count();
        u64 global_total_facts = 0;
        MPI_Allreduce(&local_facts, &global_total_facts, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, mcomm.get_local_comm());

        if (mcomm.get_local_rank() == 0)
            std::cout << curr_relation->get_debug_id() << ": {" << curr_relation->get_arity() << "}. (" << global_total_facts << " total facts)" << std::endl;

        total_facts = total_facts + global_total_facts;
    }
    if (mcomm.get_local_rank() == 0)
        std::cout << "Total facts across all relations " << total_facts << std::endl;
}
#endif



void LIE::print_all_relation_size()
{
    u64 total_facts=0;
    u64 local_facts[lie_relation_count];
    for (u32 i = 0 ; i < lie_relation_count; i++)
    {
        relation* curr_relation = lie_relations[i];
        local_facts[i] = curr_relation->get_full_element_count();
    }

    u64 global_total_facts[lie_relation_count];
    MPI_Allreduce(local_facts, global_total_facts, lie_relation_count, MPI_UNSIGNED_LONG_LONG, MPI_SUM, mcomm.get_local_comm());
    for (u32 i = 0 ; i < lie_relation_count; i++)
    {
        relation* curr_relation = lie_relations[i];
        if (mcomm.get_local_rank() == 0)
            std::cout << curr_relation->get_debug_id() << ": {" << curr_relation->get_arity() << "}. (" << global_total_facts[i] << " total facts)" << std::endl;
        total_facts = total_facts + global_total_facts[i];
    }

    if (mcomm.get_local_rank() == 0)
        std::cout << "Total facts across all relations " << total_facts << std::endl << std::endl;
}



void LIE::write_checkpoint_dump(int loop_counter, std::vector<int> executed_scc_id, int scc_id)
{
    std::string dir_name;
    dir_name = output_dir + "/checkpoint-" + std::to_string(scc_id) + "-" + std::to_string(loop_counter);

    std::string scc_metadata;
    scc_metadata = dir_name + "/scc_metadata";
	if (mcomm.get_local_rank() == 0)
	{
        //mkdir(dir_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        FILE *fp;
        fp = fopen(scc_metadata.c_str(), "w");
        for (int i = 0; i < (int)executed_scc_id.size(); i++)
            fprintf (fp, "%d\n", executed_scc_id[i]);
        fclose(fp);
	}
	MPI_Barrier(mcomm.get_local_comm());

	for (u32 i = 0 ; i < lie_relation_count; i++)
    {
        //std::cout << "Name " << lie_relations[i]->get_filename() << std::endl;
        //if(lie_relations[i]->get_debug_id() == "rel_path_2_1_2")
            lie_relations[i]->parallel_IO(dir_name);
    }
}



void LIE::create_checkpoint_dump(int loop_counter, int scc_id)
{
    std::string dir_name;
    dir_name = output_dir + "/checkpoint-" + std::to_string(scc_id) + "-" + std::to_string(loop_counter);

    std::string scc_metadata;
    scc_metadata = dir_name + "/scc_metadata";
    if (mcomm.get_local_rank() == 0)
        mkdir(dir_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    MPI_Barrier(mcomm.get_local_comm());
}



bool LIE::execute ()
{
    /// Main : Execute : init : start
    mcomm.set_local_comm(MPI_COMM_WORLD);

#ifdef GOOGLE_MAP
    if (mcomm.get_local_rank() == 0)
        std::cout << "Using Google maps"  << std::endl;
#else
    if (mcomm.get_local_rank() == 0)
        std::cout << "Using Tom's Shmap" << std::endl;
#endif

    /// Initialize all relations
    for (u32 i = 0 ; i < lie_relation_count; i++)
    {
    	lie_relations[i]->set_restart_flag(restart_flag);
    	lie_relations[i]->set_share_io(share_io);
    	lie_relations[i]->set_separate_io(separate_io);
    	lie_relations[i]->set_offset_io(offset_io);
        lie_relations[i]->initialize_relation(mcomm);

#if DEBUG_OUTPUT
        //lie_relations[i]->print();
#endif
    }

    /// create output directory for checkpoint dumps
	if (enable_io == true && mcomm.get_local_rank() == 0)
        mkdir(output_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

#if DEBUG_OUTPUT
    if (mcomm.get_local_rank() == 0)
    {
    	if (restart_flag == true)
    	{
    		if (separate_io == true)
    			std::cout << "Read Local Data without MetaData" << std::endl;
    		else if (offset_io == true)
    			std::cout << "Read Global Data with Offset MetaData" << std::endl;
    		else
    			std::cout << "Read Global Data with Size MetaData" << std::endl;
    	}
    	else
    	{
    		if (separate_io == true)
    			std::cout << "Write Local Data without MetaData" << std::endl;
    		else if (offset_io == true)
    			std::cout << "Write Global Data with Offset MetaData" << std::endl;
    		else
    			std::cout << "Write Global Data with Size MetaData" << std::endl;
    	}
        std::cout << "----------------- Initialization Complete ---------------------" << std::endl << std::endl;
    }
#endif

    /// Executable task
    RAM* executable_task = one_runnable_tasks();



    std::vector<int> executed_scc_id;  /* the sccs that have been executed */
    /* Read scc metadate if it restarts from checkpoint */
    if (restart_flag == true)
    {
        int a;
        std::string scc_metadata;
        scc_metadata = restart_dir_name + "/scc_metadata";
    	std::ifstream file(scc_metadata);
    	if (file.is_open())
    	{
    		while (file >> a)
    			executed_scc_id.push_back(a);
    	}
    	file.close();
    }

    /// Running one task at a time
    while (executable_task != NULL)
    {
        int loop_counter = 0;
    	/* Skip the scc if it has been executed before */
    	int scc_id = executable_task->get_id();
    	std::vector<int>::iterator it;
    	it = find (executed_scc_id.begin(), executed_scc_id.end(), scc_id);
    	if (it != executed_scc_id.end())
    	{
            update_task_graph(executable_task);
            executable_task = one_runnable_tasks();
    		continue;
    	}

        executable_task->set_comm(mcomm);

        /// Initialize all relations
        relation** scc_relation = executable_task->get_RAM_relations();
        bool* scc_relation_status = executable_task->get_RAM_relations_status();;
        u32 scc_relation_count = executable_task->get_ram_relation_count();
        if (restart_flag == false)
        {
			for (u32 i=0; i < scc_relation_count; i++)
			{
				if (scc_relation_status[i] == true)
					scc_relation[i]->insert_full_in_delta();
			}
        }
        else
        {
        	for (u32 i = 0 ; i < scc_relation_count; i++)
        	{
                std::string delta_filename;
                delta_filename = restart_dir_name + "/" + scc_relation[i]->get_debug_id().c_str() + "_delta";

        		if (separate_io == true)
                    delta_filename = delta_filename + "_" + std::to_string(mcomm.get_local_rank());

        		scc_relation[i]->set_filename(delta_filename);
        		scc_relation[i]->set_initailization_type(0);

                int is_access = access(delta_filename.c_str(), F_OK);
        		int access_sum = 0;
        		MPI_Allreduce(&is_access, &access_sum, 1, MPI_INT, MPI_SUM, mcomm.get_local_comm());

        		if (access_sum + mcomm.get_local_nprocs() > 0)
        		{
        			if (separate_io == true)
        				scc_relation[i]->load_data_from_separate_files();
        			else if (offset_io == true)
        				scc_relation[i]->load_data_from_file_with_offset();
        			else
        				scc_relation[i]->load_data_from_file();
        		}
        	}
        }

        std::vector<u32> history;

#if DEBUG_OUTPUT
        if (mcomm.get_local_rank() == 0)
            std::cout << "-------------------Executing SCC " << executable_task->get_id() << "------------------" << std::endl;
#endif

        /// if case is for rules (acopy and copy) that requires only one iteration
        /// else case is for join rules
        double running_time=0;
        double running_intra_bucket_comm=0;
        double running_buffer_allocate=0;
        double running_local_compute=0;
        double running_all_to_all=0;
        double running_buffer_free=0;
        double running_insert_newt=0;
        double running_insert_in_full=0;
        double running_fp=0;

        double writing_checkpoint_dump_time = 0;
        int checkpoint_dumps_num = 0;

        /// For SCCs that runs for only one iteration
        if (executable_task->get_iteration_count() == 1)
        {
            if (enable_io == true && loop_counter % cp_iteration == 0)
                create_checkpoint_dump(loop_counter, executable_task->get_id());

            //executable_task->execute_in_batches_with_all_to_all_threshold(batch_size, history, intern_map, &running_time, &running_intra_bucket_comm, &running_buffer_allocate, &running_local_compute, &running_all_to_all, &running_buffer_free, &running_insert_newt, &running_insert_in_full);
            executable_task->execute_in_batches(app_name, batch_size, history, intern_map, &running_time, &running_intra_bucket_comm, &running_buffer_allocate, &running_local_compute, &running_all_to_all, &running_buffer_free, &running_insert_newt, &running_insert_in_full, &running_fp, loop_counter, executable_task->get_id(), output_dir, all_to_all_meta_data_dump);

            executed_scc_id.push_back(executable_task->get_id());
            if (enable_io == true && loop_counter % cp_iteration == 0)
            {
            	double write_cp_start = MPI_Wtime();
                write_checkpoint_dump(loop_counter, executed_scc_id, executable_task->get_id());
                double write_cp_end = MPI_Wtime();
                writing_checkpoint_dump_time = (write_cp_end - write_cp_start);
                double max_write_cp_time = 0;
                MPI_Reduce(&writing_checkpoint_dump_time, &max_write_cp_time, 1, MPI_DOUBLE, MPI_MAX, 0, mcomm.get_comm());
                //if (mcomm.get_local_rank() == 0)
                //	std::cout << "Writing checkpoint dump " << checkpoint_dumps_num << " takes " << max_write_cp_time << "(s)" << std::endl;
                checkpoint_dumps_num++;
            }
            loop_counter++;

#if DEBUG_OUTPUT
            //for (u32 i = 0 ; i < scc_relation_count; i++)
            //    scc_relation[i]->print();
            print_all_relation_size();
#endif
        }
        /// For SCCs that runs till fixed point is reached
        else
        {
            if (mcomm.get_rank() == 0)
                std::cout << "Iteration#\tBuffer_creation_time\tComputation_time\tAll_to_all_time\tBuffer_free_time\tInsert_in_newt_time\tIntra_comm_time\tInsert_in_full_time\tTotal_time" << std::endl;
            u64 delta_in_scc = 0;
            do
            {
                if (enable_io == true && loop_counter % cp_iteration == 0)
                    create_checkpoint_dump(loop_counter, executable_task->get_id());

                //executable_task->execute_in_batches_with_all_to_all_threshold(batch_size, history, intern_map, &running_time, &running_intra_bucket_comm, &running_buffer_allocate, &running_local_compute, &running_all_to_all, &running_buffer_free, &running_insert_newt, &running_insert_in_full);
                executable_task->execute_in_batches(app_name, batch_size, history, intern_map, &running_time, &running_intra_bucket_comm, &running_buffer_allocate, &running_local_compute, &running_all_to_all, &running_buffer_free, &running_insert_newt, &running_insert_in_full, &running_fp, loop_counter, executable_task->get_id(), output_dir, all_to_all_meta_data_dump);
                //executable_task->print_all_relation();

                delta_in_scc = history[history.size()-2];
                if (delta_in_scc == 0)
                	executed_scc_id.push_back(executable_task->get_id());
                if (enable_io == true && loop_counter % cp_iteration == 0)
                {
                	double write_cp_start = MPI_Wtime();
                    write_checkpoint_dump(loop_counter, executed_scc_id, executable_task->get_id());
                    double write_cp_end = MPI_Wtime();
                    writing_checkpoint_dump_time = (write_cp_end - write_cp_start);
                    double max_write_cp_time = 0;
                    MPI_Reduce(&writing_checkpoint_dump_time, &max_write_cp_time, 1, MPI_DOUBLE, MPI_MAX, 0, mcomm.get_comm());
                    //if (mcomm.get_local_rank() == 0)
                    //    std::cout << "Writing checkpoint dump " << checkpoint_dumps_num << " takes " << max_write_cp_time << "(s)" << std::endl;
                    checkpoint_dumps_num++;
                }
                loop_counter++;


#if DEBUG_OUTPUT
                //for (u32 i = 0 ; i < scc_relation_count; i++)
                //    scc_relation[i]->print();
                //print_all_relation_size();
#endif
            }
            while (delta_in_scc != 0);
        }

        set_executed_scc_id(executed_scc_id);


        set_loop_counter(loop_counter);


        executable_task->insert_delta_in_full();

        /// marks executable_task as finished
        update_task_graph(executable_task);



        /// loads new runnable task
        executable_task = one_runnable_tasks();
    }





    return true;
}


LIE::~LIE ()
{
    for (u32 i = 0 ; i < lie_relation_count; i++)
    {
        lie_relations[i]->finalize_relation();
        delete (lie_relations[i]);
    }
}
