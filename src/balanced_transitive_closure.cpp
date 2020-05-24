#include "comm.h"
#include "RA_tasks.h"



/// Logical_Inferencing_Engine
///     SCC graphs (vector of set of SCC)
///     SCC

/// 

int main(int argc, char **argv)
{
    mpi_comm mcomm;
    mcomm.create(argc, argv);

    relation* G = new relation();
    relation* T = new relation();


    G->initialize_relation(2, mcomm);
    G->read_from_file(argv[1], FULL);
    T->initialize_relation(2, mcomm);


    parallel_copy* copy_1 = new parallel_copy();
    copy_1->set_comm(mcomm);
    copy_1->set_copy_input(G, FULL);
    copy_1->set_copy_output(T);
    int rename_index_array[2] = {1, 0};
    int rename_index_array_size = 1;
    copy_1->set_copy_rename_index(rename_index_array, rename_index_array_size);
    RAM scc0;
    scc0.push_relation(G);
    scc0.push_relation(T);
    scc0.set_comm(mcomm);
    scc0.set_iteration_count(1);
    scc0.push_back(copy_1);

    scc0.execute();



    //T->read_from_relation(T, DELTA);
    //T->flush_full();

    parallel_join* join_1 = new parallel_join();
    join_1->set_comm(mcomm);
    join_1->set_join_input0(G, FULL);
    join_1->set_join_input1(T, DELTA);
    join_1->set_join_output(T);
    int project_and_rename_index_array[3] = {-1, 0, 1};
    int project_and_rename_index_array_size = 3;
    join_1->set_join_projection_index(project_and_rename_index_array, project_and_rename_index_array_size);
    join_1->set_join_column_count(1);


    RAM scc1;
    scc1.push_relation(G);
    scc1.push_relation(T);
    scc1.set_comm(mcomm);
    scc1.push_back(join_1);
    scc1.execute();

    delete join_1;
    delete copy_1;


    delete G;
    delete T;


    mcomm.destroy();

    return 0;
}
