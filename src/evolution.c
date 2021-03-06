/**
 * Evolution - an evolutionary algorithm library
 *
 * Copyright (C)  2014  The Gapcoin developers  <info@gapcoin.org>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef EVOLUTION
#define EVOLUTION
#include "evolution.h"
#include "C-Utils/Debug/src/debug.h"


/* Evolution mutex */
static pthread_mutex_t ev_mutex = PTHREAD_MUTEX_INITIALIZER;

/********************/
/* static functions */
/********************/

/**
 * Initializes Thread Clients and Individuals
 */
static void ev_init_tc_and_ivs(Evolution *ev);

/**
 * Returns wether the given EvInitArgs are valid or not
 */
static char valid_args(EvInitArgs *args);

/**
 * Returns if a given flag combination is invalid
 */
static char ev_flags_invalid(uint64_t flags);

/**
 * Parallel init_iv function
 */
static void *threadable_init_iv(void *arg);

/**
 * Parallel greedy init iv function
 */
static void *threadable_greedy_init_iv(void *arg);

/**
 * Recombinates individual dunring an Generation change
 */
static inline void ev_init_recombinate(Evolution *ev);

/**
 * Mutate all individuals if we don't use recombination
 */
static inline void ev_init_mutate(Evolution *ev);

/**
 * in greedy mode we have on greedy best individual
 * one generation best and one temporary individual
 */
static inline void ev_init_greedy(Evolution *ev);

/**
 * switch old and new generation to discard the old one
 * which will be overidden at the next generation change
 */
static inline void ev_switch_ivs(Evolution *ev);

/**
 * Wakeup the Threadabel functions
 * an waits untill all work is done
 * and count the improovs of the
 * progressed generation
 */
static inline void evolute_ivs(Evolution *ev);

/**
 * Wakeup the Threadabel functions
 * an waits untill all work is done
 * and count the improovs of the
 * progressed generation
 *
 * set the best greedy individual as greedy individual
 * for the next generation
 */
static inline void greedy_ivs(Evolution *ev);

/**
 * Initializes the evolution process
 * by configurating and starting the threads
 * and choosing what to be done
 */
static inline void init_evolute(Evolution *ev);

/**
 * Finishes the Evolution progresse by
 * shutting down the threads
 */
static inline void close_evolute(Evolution *ev);

/**
 * Parallel recombinate
 */
static void *threadable_recombinate(void *arg);

/**
 * Thread function to do mutation onely
 * if number of survivors == number of deaths
 */
static void *threadable_mutation_onely_1half(void *arg);

/**
 * Thread function to do mutation onely
 * if numbers of survivors != number of deaths
 */
static void *threadable_mutation_onely_rand(void *arg);

/**
 * Thread function to do greedy 
 */
static void *threadable_greedy(void *arg);

/**
 * Initializes Thread Clients and Individuals serialized
 */
static void ev_init_tc_and_ivs_serial(Evolution *ev);

/**
 * Initializes Thread Clients and Individuals serialized
 */
static void ev_init_tc_and_ivs_greedy_serial(Evolution *ev);

/**
 * progress on generation on the ivs
 */
static inline void evolute_ivs_seriel(Evolution *ev);

/**
 * progress on generation on the ivs
 */
static inline void greedy_ivs_seriel(Evolution *ev);

/**
 * Parallel recombinate
 */
static void seriel_recombinate(Evolution *ev);

/**
 * Thread function to do mutation onely
 * if number of survivors == number of deaths
 */
static void seriel_mutation_onely_1half(Evolution *ev);

/**
 * Thread function to do mutation onely
 * if numbers of survivors != number of deaths
 */
static void seriel_mutation_onely_rand(Evolution *ev);

/**
 * Functions for sorting the population by fitness
 * macro versions
 */
static inline char ev_bigger(Individual *a, Individual *b) {
  return a->fitness > b->fitness;
}

static inline char ev_smaler(Individual *a, Individual *b) {
  return a->fitness < b->fitness;
}

static inline char ev_equal(Individual *a, Individual *b) {
  return a->fitness == b->fitness;
}

/**
 * Access the fittnes of the individual
 * at the given possition in the given Evolution
 * with the given opts
 */
#define EV_FITNESS_AT(EV, I) (EV)->population[I]->fitness 

/**
 * Calculates the fittnes of the individual
 * at the given possition in the given Evolution
 * with the given opts
 */
#define EV_CALC_FITNESS_AT(EV, I, OPTS)                                       \
  (EV)->population[I]->fitness = (EV)->fitness((EV)->population[I], OPTS)

/**
 * Macro for sorting the Evolution by fittnes 
 * using Macro based version onely because
 * parallel version will propably need more then
 * 16 Corse to be efficient
 */
#define EV_SELECTION(EV)                                      \
  do {                                                        \
    if ((EV)->sort_max) {                                     \
      QUICK_INSERT_SORT_MAX(Individual *,                     \
                            (EV)->population,                 \
                            (EV)->population_size,            \
                            ev_bigger,                        \
                            ev_smaler,                        \
                            ev_equal,                         \
                            (EV)->min_quicksort);             \
    } else {                                                  \
      QUICK_INSERT_SORT_MIN(Individual *,                     \
                            (EV)->population,                 \
                            (EV)->population_size,            \
                            ev_bigger,                        \
                            ev_smaler,                        \
                            ev_equal,                         \
                            (EV)->min_quicksort);             \
    }                                                         \
  } while (0)

/* macro to copy one individual to an other if it is better */
#define EV_COPY_GREEDY(EV, ID_DST, ID_SRC, OPTS)                  \
do {                                                              \
  if ((EV)->sort_max) {                                           \
    if (EV_FITNESS_AT(EV, ID_DST) < EV_FITNESS_AT(EV, ID_SRC)) {  \
      (EV)->clone_iv((EV)->population[ID_DST]->iv,                \
                     (EV)->population[ID_SRC]->iv,                \
                     OPTS);                                       \
                                                                  \
      EV_FITNESS_AT(EV, ID_DST) = EV_FITNESS_AT(EV, ID_SRC);      \
    }                                                             \
  } else {                                                        \
    if (EV_FITNESS_AT(EV, ID_DST) > EV_FITNESS_AT(EV, ID_SRC)) {  \
      (EV)->clone_iv((EV)->population[ID_DST]->iv,                \
                     (EV)->population[ID_SRC]->iv,                \
                     OPTS);                                       \
                                                                  \
      EV_FITNESS_AT(EV, ID_DST) = EV_FITNESS_AT(EV, ID_SRC);      \
    }                                                             \
  }                                                               \
} while (0)

/* also count improoves in this */
#define EV_COPY_GREEDY_COUNT(EV, ID_DST, ID_SRC, OPTS, COUNT)     \
do {                                                              \
  if ((EV)->sort_max) {                                           \
    if (EV_FITNESS_AT(EV, ID_DST) < EV_FITNESS_AT(EV, ID_SRC)) {  \
      (EV)->clone_iv((EV)->population[ID_DST]->iv,                \
                     (EV)->population[ID_SRC]->iv,                \
                     OPTS);                                       \
                                                                  \
      EV_FITNESS_AT(EV, ID_DST) = EV_FITNESS_AT(EV, ID_SRC);      \
      (COUNT)++;                                                  \
    }                                                             \
  } else {                                                        \
    if (EV_FITNESS_AT(EV, ID_DST) > EV_FITNESS_AT(EV, ID_SRC)) {  \
      (EV)->clone_iv((EV)->population[ID_DST]->iv,                \
                     (EV)->population[ID_SRC]->iv,                \
                     OPTS);                                       \
                                                                  \
      EV_FITNESS_AT(EV, ID_DST) = EV_FITNESS_AT(EV, ID_SRC);      \
      (COUNT)++;                                                  \
    }                                                             \
  }                                                               \
} while (0)


/**
 * Prints status information during 
 * threadable init individuals 
 */
#define EV_INIT_IV_OUTPUT(INDEX)                                              \
  do {                                                                        \
    pthread_mutex_lock(&ev_mutex);                                            \
    printf("init population: %10d\r", INDEX);                                 \
    pthread_mutex_unlock(&ev_mutex);                                          \
  } while (0)

/**
 * Prints status information during 
 * threadable recombination and mutation TODO make seperate output vor recombination mut_1/2
 */
#define EV_IV_STATUS_OUTPUT(EV, INDEX)                                        \
  do {                                                                        \
      pthread_mutex_lock(&ev_mutex);                                          \
      printf("Evolution: generation left %10d "                               \
             "tasks mutation-1/x %10d improovs %15.5f%%\r",                   \
             (EV).generation_limit - (EV).info.generations_progressed,        \
             (EV).overall_end - INDEX,                                        \
             (((EV).info.improovs / (double) (EV).deaths) * 100.0));          \
      pthread_mutex_unlock(&ev_mutex);                                        \
  } while (0)

/**
 * Prints status information during 
 * threadable greedy
 */
#define EV_IV_GREEDY_STATUS_OUTPUT(EV, INDEX)                                 \
  do {                                                                        \
      pthread_mutex_lock(&ev_mutex);                                          \
      printf("Evolution: generation left %10d "                               \
             "tasks greedy %10d improovs %10.d\r",                            \
             (EV).generation_limit - (EV).info.generations_progressed,        \
             (EV).greedy_size - INDEX,                                        \
             (EV).info.improovs);                                             \
      pthread_mutex_unlock(&ev_mutex);                                        \
  } while (0)

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/**
 * Prints status information during 
 * threadable recombination and mutation TODO make seperate output vor recombination mut_1/2
 */
#define EV_IV_STATUS_OUTPUT_SERIEL(EV, INDEX)                                 \
  do {                                                                        \
      printf("Evolution: generation left %10d "                               \
             "tasks mutation-1/x %10d improovs %15.5f%%\r",                   \
             (EV).generation_limit - (EV).info.generations_progressed,        \
             (EV).overall_end - INDEX,                                        \
             (((EV).info.improovs / (double) (EV).deaths) * 100.0));          \
  } while (0)

/**
 * Prints status info during
 * main elvoute function
 */
#define EV_EVOLUTE_OUTPUT(EV)                                                 \
do {                                                                          \
  if ((EV).info.improovs) {                                                   \
    printf("\33[2K\rimproovs: " ANSI_COLOR_GREEN "%10d" ANSI_COLOR_RESET      \
           " -> %15.5f%%  best fitness: %10li\n",                             \
           (EV).info.improovs,                                                \
           (((EV).info.improovs / (double) (EV).deaths) * 100.0),             \
           (EV).population[0]->fitness);                                      \
  } else {                                                                    \
    printf("\33[2K\rimproovs: " ANSI_COLOR_RED "%10d" ANSI_COLOR_RESET        \
           " -> %15.5f%%  best fitness: %10li\n",                             \
           (EV).info.improovs,                                                \
           (((EV).info.improovs / (double) (EV).deaths) * 100.0),             \
           (EV).population[0]->fitness);                                      \
  }                                                                           \
} while (0)

/**
 * Prints status info during
 * main elvoute function (greedy version)
 */
#define EV_GREEDY_OUTPUT(EV)                                                  \
do {                                                                          \
  if ((EV).info.improovs) {                                                   \
    printf("\33[2K\rimproovs: " ANSI_COLOR_GREEN "%10d" ANSI_COLOR_RESET      \
           " -> best fitness: %10li\n",                                       \
           (EV).info.improovs,                                                \
           (EV).population[0]->fitness);                                      \
  } else {                                                                    \
    printf("\33[2K\rimproovs: " ANSI_COLOR_RED "%10d" ANSI_COLOR_RESET        \
           " -> best fitness: %10li\n",                                       \
           (EV).info.improovs,                                                \
           (EV).population[0]->fitness);                                      \
  }                                                                           \
} while (0)

#define EV_THREAD_SAVE_NEW_LINE                                               \
  do {                                                                        \
    pthread_mutex_lock(&ev_mutex);                                            \
    printf("\n");                                                             \
    pthread_mutex_unlock(&ev_mutex);                                          \
  } while (0)
    


/**
 * Sadly we have to subvert the type system to initialize an const members
 * of an struct on the heap. (there are other ways with memcpy and an static
 * initialized const struct, but thats C99 and it will get even uglier than
 * than this)
 * For explanation we have to get the address of the const value cast this
 * address to an pointer of an non const version of the specific value
 * and dereference this pointer to set the acctual const value
 */
#define INIT_C_INIT_IV(X, Y) *(void *(**)(void *))                 &(X) = (Y)
#define INIT_C_CLON_IV(X, Y) *(void (**)(void *, void *, void *))  &(X) = (Y)
#define INIT_C_FREE_IV(X, Y) *(void (**)(void *, void *))          &(X) = (Y)
#define INIT_C_MUTTATE(X, Y) *(void (**)(Individual *, void *))    &(X) = (Y)
#define INIT_C_FITNESS(X, Y) *(int64_t (**)(Individual *, void *)) &(X) = (Y)
#define INIT_C_RECOMBI(X, Y) *(void (**)(Individual *,                        \
                                         Individual *,                        \
                                         Individual *,                        \
                                         void *))                  &(X) = (Y)
#define INIT_C_CONTINU(X, Y) *(char (**)(Evolution *const))        &(X) = (Y)
#define INIT_C_EVTARGS(X, Y) *(EvThreadArgs ***)                   &(X) = (Y)
#define INIT_C_ETA(X, Y)     *(EvThreadArgs **)                    &(X) = (Y)
#define INIT_C_INT(X, Y)     *(int *)                              &(X) = (Y)
#define INIT_C_DBL(X, Y)     *(double *)                           &(X) = (Y)
#define INIT_C_OPT(X, Y)     *(void ***)                           &(X) = (Y)
#define INIT_C_TCS(X, Y)     *(TClient **)                         &(X) = (Y)
#define INIT_C_CHR(X, Y)     *(char *)                             &(X) = (Y)
#define INIT_C_U64(X, Y)     *(uint64_t *)                         &(X) = (Y)
#define INIT_C_U16(X, Y)     *(uint16_t *)                         &(X) = (Y)
#define INIT_C_EVO(X, Y)     *(Evolution **)                       &(X) = (Y)
#define INIT_C_VPT(X, Y)     *(void **)                            &(X) = (Y)

/**
 * Returns pointer to an new and initialzed Evolution
 * take pointers for an EvInitArgs struct
 *
 * see comment of EvInitArgs for more informations
 */
Evolution *new_evolution(EvInitArgs *args) {
  
  /* validates args */
  if (!valid_args(args))
    return NULL;

  /* in greedy mode we have on greedy best individual
   * one generation best and one temporary individual */
  if (args->flags & EV_GRDY)
    args->population_size = 3 * args->num_threads;

  /* create new Evolution */
  Evolution *ev = (Evolution *) malloc(sizeof(Evolution));

  /* int random */
  ev->rands = (rand128_t **) malloc(sizeof(rand128_t *) * args->num_threads);
  int i;
  for (i = 0; i < args->num_threads; i++)
    ev->rands[i] = new_rand128(time(NULL) ^ i);

  /**
   * multiplicator: if we should discard the last generation, 
   * we can't recombinate in place
   */
  int mul = (args->flags & EV_KEEP) ? 1 : 2;
  mul = (args->flags & EV_GRDY) ? 1 : mul;

  size_t population_space  = sizeof(Individual *); 
  size_t ivs_space         = sizeof(Individual); 

  population_space         *= args->population_size * mul;
  ivs_space                *= args->population_size * mul;
  
                           
  ev->population           = (Individual **) malloc(population_space);
  ev->ivs                  = (Individual *) malloc(ivs_space);

  INIT_C_INIT_IV(ev->init_iv,     args->init_iv);
  INIT_C_CLON_IV(ev->clone_iv,    args->clone_iv);
  INIT_C_FREE_IV(ev->free_iv,     args->free_iv);
  INIT_C_MUTTATE(ev->mutate,      args->mutate);
  INIT_C_FITNESS(ev->fitness,     args->fitness);
  INIT_C_RECOMBI(ev->recombinate, args->recombinate);
  INIT_C_CONTINU(ev->continue_ev, args->continue_ev);

  /* only needed if we have more than one thread */
  if (args->num_threads > 1) {
    INIT_C_TCS(ev->thread_clients,  malloc(sizeof(TClient) * 
                                           args->num_threads));

    INIT_C_EVTARGS(ev->thread_args, malloc(sizeof(EvThreadArgs *) *
                                           args->num_threads));
  }

  INIT_C_INT(ev->population_size,       args->population_size);
  INIT_C_INT(ev->greedy_size,           args->greedy_size);
  INIT_C_INT(ev->greedy_individuals,    args->greedy_individuals);
  INIT_C_INT(ev->generation_limit,      args->generation_limit);
  INIT_C_DBL(ev->mutation_propability,  args->mutation_propability);
  INIT_C_DBL(ev->death_percentage,      args->death_percentage);
  INIT_C_OPT(ev->opts,                  args->opts);
  INIT_C_INT(ev->num_threads,           args->num_threads);

  INIT_C_INT(ev->i_mut_propability,     (int) ((double) RAND_MAX * 
                                               ev->mutation_propability));

  INIT_C_CHR(ev->use_recombination,     args->flags & EV_UREC);
  INIT_C_CHR(ev->use_muttation,         args->flags & EV_UMUT);
  INIT_C_CHR(ev->always_mutate,         args->flags & EV_AMUT);
  INIT_C_CHR(ev->keep_last_generation,  args->flags & EV_KEEP);
  INIT_C_CHR(ev->use_abort_requirement, args->flags & EV_ABRT);
  INIT_C_CHR(ev->use_greedy,            args->flags & EV_GRDY);
  INIT_C_CHR(ev->sort_max,              args->flags & EV_SMAX);
  INIT_C_U16(ev->verbose,               args->flags & (EV_VEB1 |
                                                       EV_VEB2 |
                                                       EV_VEB3));

  INIT_C_INT(ev->min_quicksort,         EV_QICKSORT_MIN);
  INIT_C_INT(ev->deaths,                (int) ((double) ev->population_size * 
                                               ev->death_percentage));
  INIT_C_INT(ev->survivors,             ev->population_size - ev->deaths);

  ev->info.improovs                     = 0;
  ev->info.generations_progressed       = 0;

  /**
   * Initializes Thread Clients and Individuals
   */
  if (args->num_threads > 1)
    ev_init_tc_and_ivs(ev);
  else if (ev->use_greedy)
    ev_init_tc_and_ivs_greedy_serial(ev);
  else
    ev_init_tc_and_ivs_serial(ev);

  return ev;
}

/**
 * Initializes Thread Clients and Individuals
 */
static void ev_init_tc_and_ivs(Evolution *ev) {

  /**
   * init thread lib
   * max work will be death_percentage * num individuals
   */
  int i;
  for (i = 0; i < ev->num_threads; i++)
    init_thread_client(&ev->thread_clients[i]);

  /**
   * multiplicator: if we should discard the last generation, 
   * we can't recombinate in place
   */
  int mul = ev->keep_last_generation ? 1 : 2;
  mul = (ev->use_greedy ? 1 : mul);

  /* start and end of calculation */
  ev->overall_start = 0;
  ev->overall_end   = ev->population_size * mul;

  /**
   * number of individuals calculated by one thread
   */
  uint32_t ivs_per_thread = (ev->overall_end - ev->overall_start) /
                            ev->num_threads + 1;

  /* in greedy mode we have on greedy best individual
   * one generation best and one temporary individual */
  if (ev->use_greedy)
    ivs_per_thread = 3;

  /* add work for the clients */
  for (i = 0; i < ev->num_threads; i++) {

    /* init thread args */
    INIT_C_ETA(ev->thread_args[i], malloc(sizeof(EvThreadArgs)));
    INIT_C_EVO(ev->thread_args[i]->ev,    ev);
    INIT_C_INT(ev->thread_args[i]->index, i);
    INIT_C_VPT(ev->thread_args[i]->opt,   ev->opts[i]);
    ev->thread_args[i]->improovs = 0;

    /* set working area of thread i */
    ev->thread_args[i]->start = ev->overall_start + i * ivs_per_thread;
    ev->thread_args[i]->end   = ev->thread_args[i]->start + ivs_per_thread;

    if (ev->thread_args[i]->end > ev->overall_end)
      ev->thread_args[i]->end = ev->overall_end;

    if (ev->thread_args[i]->start > ev->overall_end)
      ev->thread_args[i]->start = ev->overall_end;

    tc_add_func(&ev->thread_clients[i], 
                (ev->use_greedy ? threadable_greedy_init_iv : 
                                  threadable_init_iv), 
                (void *) ev->thread_args[i]);
  }

  /* wait for threads and collect improovs */
  for (i = 0; i < ev->num_threads; i++) {
    tc_join(&ev->thread_clients[i]);
    ev->info.improovs += ev->thread_args[i]->improovs;
  }

  /**
   * Select the best individual to survive,
   * Sort the Individuals by their fittnes
   */
  if (!ev->use_greedy)
    EV_SELECTION(ev); 

  if (ev->verbose >= EV_VERBOSE_HIGH)
    printf("Population Initialized\n");

}

/**
 * Undef const initializer to avoid reusing it
 */
#undef INIT_C_INIT_IV  
#undef INIT_C_CLON_IV 
#undef INIT_C_FREE_IV  
#undef INIT_C_MUTTATE   
#undef INIT_C_FITNESS  
#undef INIT_C_RECOMBI    
#undef INIT_C_CONTINU   
#undef INIT_C_EVTARGS
#undef INIT_C_INT      
#undef INIT_C_DBL      
#undef INIT_C_OPT     
#undef INIT_C_TCS      
#undef INIT_C_CHR      
#undef INIT_C_U64 
#undef INIT_C_U16
#undef INIT_C_EVO
#undef INIT_C_VPT

/**
 * Returns wether the given EvInitArgs are valid or not
 */
static char valid_args(EvInitArgs *args) {

  if (ev_flags_invalid(args->flags)) {
    DBG_MSG("wrong flag combination");
    return 0;
  }

  /* valid other opts */
  if (args->num_threads          <   1 ||
      args->population_size      <=  0 ||
      args->generation_limit     <   0 ||
      args->mutation_propability < 0.0 ||
      args->mutation_propability > 1.0 ||
      args->death_percentage     < 0.0 ||
      args->death_percentage     > 1.0) {
   
    DBG_MSG("wrong opts");
    return 0;
  }

  if (args->flags & EV_GRDY            && (
       args->greedy_size         <=  0 ||
       args->greedy_individuals  <=  0)) {

    DBG_MSG("wrong opts");
    return 0;
  }

  if (args->opts == NULL)
    args->opts = (void**) malloc(sizeof(void *) * args->num_threads);

  return 1;
}


/**
 * Returns if a given flag combination is invalid
 */
static char ev_flags_invalid(uint64_t flags) {

  /**
   * Sorting and verbose flags
   * can be used with any flag combination
   */
  int tflags = flags & ~EV_SMAX;
  tflags &= ~EV_VEB1;
  tflags &= ~EV_VEB2;
  tflags &= ~EV_VEB3;
  
  return tflags != EV_UREC                                   &&
         tflags != (EV_UREC|EV_UMUT)                         &&
         tflags != (EV_UMUT|EV_AMUT)                         &&
         tflags != (EV_UREC|EV_UMUT|EV_AMUT)                 &&
         tflags != (EV_UREC|EV_KEEP)                         &&
         tflags != (EV_UREC|EV_UMUT|EV_KEEP)                 &&
         tflags != (EV_UMUT|EV_AMUT|EV_KEEP)                 &&
         tflags != (EV_UREC|EV_UMUT|EV_AMUT|EV_KEEP)         &&
         tflags != (EV_UREC|EV_ABRT)                         &&
         tflags != (EV_UREC|EV_UMUT|EV_ABRT)                 &&
         tflags != (EV_UMUT|EV_AMUT|EV_ABRT)                 &&
         tflags != (EV_UREC|EV_UMUT|EV_AMUT|EV_ABRT)         &&
         tflags != (EV_UREC|EV_KEEP|EV_ABRT)                 &&
         tflags != (EV_UREC|EV_UMUT|EV_KEEP|EV_ABRT)         &&
         tflags != (EV_UMUT|EV_AMUT|EV_KEEP|EV_ABRT)         &&
         tflags != (EV_UREC|EV_UMUT|EV_AMUT|EV_KEEP|EV_ABRT) &&
         tflags != (EV_GRDY|EV_UMUT|EV_AMUT)                 && 
         tflags != (EV_GRDY|EV_UMUT|EV_AMUT|EV_ABRT);

}

/**
 * Frees unneded resauces after an evolution calculation
 * Note it don't touches the spaces of the best individual
 */
void evolution_clean_up(Evolution *ev) {

  int end = ev->keep_last_generation ? ev->population_size : 
                                       ev->population_size * 2; 
  end = (ev->use_greedy ? ev->population_size : end);
  int i;

  /**
   * free individuals starting by index one because
   * zero is the best individual
   */
  for (i = 1; i < end; i++) 
    ev->free_iv(ev->population[i]->iv, 
                ev->opts[i]);

  free(ev->ivs);
  free(ev->population);

  /* free copys from the threads */
  for (i = 0; i < ev->num_threads && ev->num_threads > 1; i++) {
    free(ev->thread_args[i]);
    tc_free(&ev->thread_clients[i]);
    free(ev->rands[i]);
  }
  
  if (ev->num_threads > 1) {
    free((void *) ev->thread_args);
    free(ev->thread_clients);
  }

  free(ev->rands);
}

/**
 * Computes an evolution for the given args
 * and returns the best Individual
 */
Individual best_evolution(EvInitArgs *args) {

  Evolution *ev   = new_evolution(args);
  Individual best = *evolute(ev);
  evolution_clean_up(ev);
  free(ev);

  return best;
}

/**
 * Parallel init_iv function
 */
static void *threadable_init_iv(void *arg) {

  EvThreadArgs *evt = arg;
  Evolution *ev     = evt->ev;
  int i;

  /**
   * Loop untill all individuals of this thread are initialized
   */
  for (i = evt->start; i < evt->end; i++) {
     
    /**
     * create new individual
     */
    ev->population[i]     = ev->ivs + i;
    ev->population[i]->iv = ev->init_iv(evt->opt);
    EV_CALC_FITNESS_AT(ev, i, evt->opt);

    /**
     * prints status informations if wanted
     */
    if (ev->verbose >= EV_VERBOSE_ONELINE) {
      EV_INIT_IV_OUTPUT(i);

      if (ev->verbose >= EV_VERBOSE_HIGH)
        EV_THREAD_SAVE_NEW_LINE;
    }
  }

  return NULL;
}

/**
 * Parallel greedy init iv function
 */
static void *threadable_greedy_init_iv(void *arg) {

  EvThreadArgs *evt = arg;
  Evolution *ev     = evt->ev;
  int i, start = evt->start;

  /**
   * create new individual
   */
  for (i = 0; i < 3; i++) {
    ev->population[start + i]          = ev->ivs + start + i;
    ev->population[start + i]->iv      = ev->init_iv(evt->opt);
    EV_CALC_FITNESS_AT(ev, start + i, evt->opt);
  }

  /**
   * generate random individuals and choose the best 
   */
  for (i = 0; i < ev->greedy_individuals; i++) {
     
    /**
     * create new individual
     */
    ev->free_iv(ev->population[start + 1]->iv, evt->opt);
    ev->population[start + 1]->iv = ev->init_iv(evt->opt);
    EV_CALC_FITNESS_AT(ev, start + 1, evt->opt);

    /* set best individual if neccesary */
    EV_COPY_GREEDY_COUNT(ev, start, start + 1, evt->opt, evt->improovs);

    /**
     * prints status informations if wanted
     */
    if (ev->verbose >= EV_VERBOSE_ONELINE) {
      EV_INIT_IV_OUTPUT(i);

      if (ev->verbose >= EV_VERBOSE_HIGH)
        EV_THREAD_SAVE_NEW_LINE;
    }
  }

  return NULL;
}

/**
 * Recombinates individual dunring an Generation change
 */
static inline void ev_init_recombinate(Evolution *ev) {

  int j;

  /**
   * If we keep the last generation, we can recombinate in place
   * else we first calculate an new population (start -> end,
   * is the area where the individuals will be overidden by new ones
   */
  if (ev->keep_last_generation) {
    ev->overall_start = ev->survivors;
    ev->overall_end   = ev->population_size;
  } else {
    ev->overall_start = ev->population_size;
    ev->overall_end   = ev->population_size * 2;
  }

  /* break if we using serial version */
  if (ev->num_threads <= 1) return;


  /**
   * number of individuals calculated by one thread
   */
  uint32_t ivs_per_thread = (ev->overall_end - ev->overall_start) /
                            ev->num_threads + 1;

  /* start parallel working */
  for (j = 0; j < ev->num_threads; j++) {

    ev->thread_args[j]->start = ev->overall_start + j * ivs_per_thread;
    ev->thread_args[j]->end   = ev->thread_args[j]->start + ivs_per_thread;

    if (ev->thread_args[j]->end > ev->overall_end)
      ev->thread_args[j]->end = ev->overall_end;

    if (ev->thread_args[j]->start > ev->overall_end)
      ev->thread_args[j]->start = ev->overall_end;

    tc_set_rerun_func(&ev->thread_clients[j], 
                      threadable_recombinate, 
                      (void *) ev->thread_args[j]);
  }

}

/**
 * Mutate all individuals if we don't use recombination
 */
static inline void ev_init_mutate(Evolution *ev) {

  int j;

  /**
   * If we keep the last generation, we can recombinate in place
   * else we first calculate an new population (start -> end,
   * is the area where the individuals will be overidden by new ones
   */
  if (ev->keep_last_generation) {
    ev->overall_start = ev->survivors;
    ev->overall_end   = ev->population_size;
  } else {
    ev->overall_start = ev->population_size;
    ev->overall_end   = ev->population_size * 2;
  }
  
  /* break if we using serial version */
  if (ev->num_threads <= 1) return;

  /**
   * number of individuals calculated by one thread
   */
  uint32_t ivs_per_thread = (ev->overall_end - ev->overall_start) /
                            ev->num_threads + 1;

  /* setting start and end areas for each thread */
  for (j = 0; j < ev->num_threads; j++) {

    ev->thread_args[j]->start = ev->overall_start + j * ivs_per_thread;
    ev->thread_args[j]->end   = ev->thread_args[j]->start + ivs_per_thread;

    if (ev->thread_args[j]->end > ev->overall_end)
      ev->thread_args[j]->end = ev->overall_end;

    if (ev->thread_args[j]->start > ev->overall_end)
      ev->thread_args[j]->start = ev->overall_end;
  }
     
  /**
   * if deaths == survivors, making sure that all survivers 
   * are being copyed and mutated
   */
  if (ev->overall_start * 2 == ev->overall_end) {

    /* start parallel working */
    for (j = 0; j < ev->num_threads; j++) {

      tc_set_rerun_func(&ev->thread_clients[j], 
                        threadable_mutation_onely_1half, 
                        (void *) ev->thread_args[j]);
    }

  /* else choose random survivers to mutate */
  } else {

    /* start parallel working */
    for (j = 0; j < ev->num_threads; j++) {

      tc_set_rerun_func(&ev->thread_clients[j], 
                        threadable_mutation_onely_rand, 
                        (void *) ev->thread_args[j]);
    }
  }

}

/**
 * in greedy mode we have on greedy best individual
 * one generation best and one temporary individual
 */
static inline void ev_init_greedy(Evolution *ev) {

  int j;

  ev->overall_start = 0;
  ev->overall_end   = ev->population_size;
  
  /* break if we using serial version */
  if (ev->num_threads <= 1) return;

  /**
   * number of individuals calculated by one thread
   */
  uint32_t ivs_per_thread = 3; 

  /* setting start and end areas for each thread */
  for (j = 0; j < ev->num_threads; j++) {

    ev->thread_args[j]->start = ev->overall_start + j * ivs_per_thread;
    ev->thread_args[j]->end   = ev->thread_args[j]->start + ivs_per_thread;

    if (ev->thread_args[j]->end > ev->overall_end)
      ev->thread_args[j]->end = ev->overall_end;

    if (ev->thread_args[j]->start > ev->overall_end)
      ev->thread_args[j]->start = ev->overall_end;
  }
     
  /* start parallel working */
  for (j = 0; j < ev->num_threads; j++) {

    tc_set_rerun_func(&ev->thread_clients[j], 
                      threadable_greedy,
                      (void *) ev->thread_args[j]);
  }
}


/**
 * switch old and new generation to discard the old one
 * which will be overidden at the next generation change
 */
static inline void ev_switch_ivs(Evolution *ev) {

  int j;
  Individual *tmp_iv;

  for (j = 0; j < ev->population_size; j++) {
    tmp_iv = ev->population[j];
    ev->population[j] = ev->population[ev->population_size + j];
    ev->population[ev->population_size + j] = tmp_iv;
  }
}

/**
 * Wakeup the Threadabel functions
 * an waits untill all work is done
 * and count the improovs of the
 * progressed generation
 */
static inline void evolute_ivs(Evolution *ev) {
  
  int j;

  /**
   * wakeup all threads
   */
  for (j = 0; j < ev->num_threads; j++) 
    tc_rerun(&ev->thread_clients[j]);

  /**
   * Wait untill all threads are finished
   */
  for (j = 0; j < ev->num_threads; j++) 
    tc_join(&ev->thread_clients[j]);

  /**
   * resets and count improovs
   * of the finished threads
   */
  ev->info.improovs = 0;

  for (j = 0; j < ev->num_threads; j++)
    ev->info.improovs += ev->thread_args[j]->improovs;

}

/**
 * Wakeup the Threadabel functions
 * an waits untill all work is done
 * and count the improovs of the
 * progressed generation
 *
 * set the best greedy individual as greedy individual
 * for the next generation
 */
static inline void greedy_ivs(Evolution *ev) {
  
  int j;
  int64_t best_fitness = ev->population[0]->fitness;
  int best_index  = 0;

  /**
   * wakeup all threads
   */
  for (j = 0; j < ev->num_threads; j++) 
    tc_rerun(&ev->thread_clients[j]);

  /**
   * Wait untill all threads are finished
   */
  for (j = 0; j < ev->num_threads; j++) 
    tc_join(&ev->thread_clients[j]);

  /**
   * resets, count improovs
   * of the finished threads
   */
  ev->info.improovs = 0;

  for (j = 0; j < ev->num_threads; j++)
    ev->info.improovs += ev->thread_args[j]->improovs;

  /* find the best greedy individual */
  for (j = 1; j < ev->num_threads; j++) {
    
    if (ev->sort_max) {
      if (ev->population[j * 3]->fitness > best_fitness) {
        best_fitness = ev->population[j * 3]->fitness;
        best_index   = j * 3;
      } 
    } else {
      if (ev->population[j * 3]->fitness < best_fitness) {
        best_fitness = ev->population[j * 3]->fitness;
        best_index   = j * 3;
      } 
    }
  }
  
  /* copy the best individual to all other threads */
  for (j = 0; j < ev->num_threads; j++) {
    if (j * 3 != best_index) {
      ev->clone_iv(ev->population[j * 3]->iv, ev->population[best_index]->iv, ev->opts[j]);
      ev->population[j + 3]->fitness = ev->population[best_index]->fitness;
    }
  }
}

/**
 * Initializes the evolution process
 * by configurating and starting the threads
 * and choosing what to be done
 */
static inline void init_evolute(Evolution *ev) {

  /**
   * init recombinatin or mutatation
   * depeding on given flags in init
   */
  if (ev->use_greedy)
    ev_init_greedy(ev);
  else if (ev->use_recombination)
    ev_init_recombinate(ev);
  else 
    ev_init_mutate(ev);
  
}

/**
 * Finishes the Evolution progresse to some verbose work if neccesary
 */
static inline void close_evolute(Evolution *ev) {

  /* clear line if neccesary */
  if (ev->verbose >= EV_VERBOSE_ONELINE)
    printf("\33[2K\r");

}

/**
 * do the actual evolution, which means
 *  - calculate the fitness for each Individual
 *  - sort Individual by fitness
 *  - remove worst ivs
 *  - grow a new generation
 *
 * Retruns the best individual
 */
Individual *evolute(Evolution *ev) {

  int i;

  /**
   * initalize the evolution process
   */
  init_evolute(ev);

  /**
   * print status informations if wanted
   */
  if (ev->verbose >= EV_VERBOSE_HIGH && ev->use_greedy)
    EV_GREEDY_OUTPUT(*ev);

  /**
   * Generation loop
   * each loop lets one generation grow kills the worst individuals
   * and let new individuals born (depending on the previous given flags)
   */
  for (i = 0; 
       i < ev->generation_limit && 
       (!ev->use_abort_requirement || 
        ev->continue_ev(ev)); 
       i++) {

    /**
     * recombinates or mutates individuals
     * depeding on given flags in init
     */
    if (ev->num_threads > 1) {
      if (ev->use_greedy)
        greedy_ivs(ev);
      else
        evolute_ivs(ev);
    } else {
      if (ev->use_greedy)
        greedy_ivs_seriel(ev);
      else
        evolute_ivs_seriel(ev);
    }
  
    /**
     * switch old and new generation to discard the old one
     * which will be overidden next time
     */
    if (!ev->keep_last_generation && !ev->use_greedy)
      ev_switch_ivs(ev);

    /**
     * Select the best individuals to survindividuale,
     * Sort the Individuals by theur fittnes
     */
    if (!ev->use_greedy)
      EV_SELECTION(ev);

    /* update progressed generations */
    ev->info.generations_progressed = i + 1;

    /**
     * print status informations if wanted
     */
    if (ev->verbose >= EV_VERBOSE_HIGH && ev->use_greedy)
      EV_GREEDY_OUTPUT(*ev);
    else if (ev->verbose >= EV_VERBOSE_HIGH)
      EV_EVOLUTE_OUTPUT(*ev);

  }

  /* shutdown threads */
  close_evolute(ev);

  return ev->population[0];
}


/**
 * Parallel recombinate
 */
static void *threadable_recombinate(void *arg) {

  EvThreadArgs *evt = arg;
  Evolution *ev     = evt->ev;
  int j, rand1, rand2;
  rand128_t *v_rand = evt->ev->rands[evt->index];

  /**
   * for recombination there musst be min two individuals
   */
  #ifdef DEBUG
  if (ev->survivors <= 1)
    ERR_MSG("less than 2 individuals in recombinate");
  #endif

  /* reset threadwide iprooves */
  evt->improovs = 0;  

  /**
   * loop untill all recombinations of this thread are done
   */
  for (j = evt->start; j < evt->end; j++) {

    /**
     * from two randomly choosen Individuals 
     * of the untouched (best) part we calculate an new one 
     * */
    rand2 = rand1 = rand128(v_rand) % ev->overall_start;
    while (rand1 == rand2) rand2 = rand128(v_rand) % ev->overall_start; 
    
    /* recombinate individuals */
    ev->recombinate(ev->population[rand1], 
                    ev->population[rand2], 
                    ev->population[j], 
                    evt->opt);
    
    /* mutate Individuals */
    if (ev->use_muttation) {
      if (ev->always_mutate)
        ev->mutate(ev->population[j], evt->opt);
      else {
        if (rand128(v_rand) <= ev->i_mut_propability)
          ev->mutate(ev->population[j], evt->opt);
      }
    }

    /* calculate the fittnes for the new individuals */
    EV_CALC_FITNESS_AT(ev, j, evt->opt);

    /**
     * store if the new individual is better as the old one
     */
    if (ev->sort_max) {
      if (ev->population[j]->fitness > ev->population[rand1]->fitness && 
          ev->population[j]->fitness > ev->population[rand2]->fitness) {

        evt->improovs++;
      }

    } else {
      if (ev->population[j]->fitness < ev->population[rand1]->fitness && 
          ev->population[j]->fitness < ev->population[rand2]->fitness) {

        evt->improovs++;
      }
    }

    /**
     * print status informations if wanted
     */
    if (ev->verbose >= EV_VERBOSE_ONELINE) {
      EV_IV_STATUS_OUTPUT(*ev, j);

      if (ev->verbose >= EV_VERBOSE_ULTRA)
        EV_THREAD_SAVE_NEW_LINE;
    }
  }

  return NULL;
}

/**
 * Thread function to do mutation onely
 * if number of survivors == number of deaths
 */
static void *threadable_mutation_onely_1half(void *arg) {

  EvThreadArgs *evt = arg;
  Evolution *ev     = evt->ev;
  int j;
  
  /* reset threadwide iprooves */
  evt->improovs = 0;  
 
  /**
   * loop untill all mutations of this thread are done
   */
  for (j = evt->start; j < evt->end; j++) {
 
    /**
     * clone the current individual (from the survivors)
     * and override an individual in the deaths-part
     */
    ev->clone_iv(ev->population[j]->iv, 
                 ev->population[j - ev->overall_start]->iv, 
                 evt->opt);
 
    /* muttate the cloned individual */
    ev->mutate(ev->population[j], 
               evt->opt);
 
    /* calculate the fittnes for the new individual */
    EV_CALC_FITNESS_AT(ev, j, evt->opt);
    
    /**
     * store if the new individual is better as the old one
     */
    if (ev->sort_max) {
      if (EV_FITNESS_AT(ev, j) > 
          EV_FITNESS_AT(ev, j - ev->overall_start)) {
 
        evt->improovs++;
      }
 
    } else {
      if (EV_FITNESS_AT(ev, j) <
          EV_FITNESS_AT(ev, j - ev->overall_start)) {
 
        evt->improovs++;
      }
    }
    
    /**
     * print status informations if wanted
     */
    if (ev->verbose >= EV_VERBOSE_ONELINE) {
      EV_IV_STATUS_OUTPUT(*ev, j);
 
      if (ev->verbose >= EV_VERBOSE_ULTRA)
        EV_THREAD_SAVE_NEW_LINE;
    }
  }

  return NULL;
}

/**
 * Thread function to do mutation onely
 * if numbers of survivors != number of deaths
 */
static void *threadable_mutation_onely_rand(void *arg) {

  EvThreadArgs *evt = arg;
  Evolution *ev = evt->ev;
  int j, rand1;
  rand128_t *v_rand = evt->ev->rands[evt->index];

  /* reset threadwide iprooves */
  evt->improovs = 0;  
 
  /**
   * loop untill all mutations are done
   */
  for (j = evt->start; j < evt->end; j++) {
 
    /**
     * clone random individual (from the survivors)
     * and override the current individual in the deaths-part
     */
    rand1 = rand128(v_rand) % ev->overall_start;
    ev->clone_iv(ev->population[j]->iv, 
                 ev->population[rand1]->iv, 
                 evt->opt);
 
    /* muttate the cloned individual */
    ev->mutate(ev->population[j], evt->opt);
 
    /* calculate the fittnes for the new individual */
    EV_CALC_FITNESS_AT(ev, j, evt->opt);
   
    /**
     * store if the new individual is better as the old one
     */
    if (ev->sort_max) {
      if (ev->population[j]->fitness > ev->population[rand1]->fitness) {
 
        evt->improovs++;
      }
 
    } else {
      if (ev->population[j]->fitness < ev->population[rand1]->fitness) {
 
        evt->improovs++;
      }
    }
   
    /**
     * print status informations if wanted
     */
    if (ev->verbose >= EV_VERBOSE_ONELINE) {
      EV_IV_STATUS_OUTPUT(*ev, j);
 
      if (ev->verbose >= EV_VERBOSE_ULTRA)
        EV_THREAD_SAVE_NEW_LINE;
    }
  }

  return NULL;
}

/**
 * Thread function to do greedy 
 */
static void *threadable_greedy(void *arg) {

  EvThreadArgs *evt = arg;
  Evolution *ev = evt->ev;
  int j, start = evt->start;

  /* reset threadwide iprooves */
  evt->improovs = 0;  
 
  /* initialize generation best to greedy best */
  ev->clone_iv(ev->population[start + 1]->iv, ev->population[start]->iv, evt->opt);
  ev->population[start + 1]->fitness = ev->population[start]->fitness;

  for (j = 0; j < ev->greedy_size; j++) {

    /* copy greedy best and mutate it */
    ev->clone_iv(ev->population[start + 2]->iv, ev->population[start]->iv, evt->opt);
    ev->mutate(ev->population[start + 2], evt->opt);

    /* calculate fitness and set generation best if neccesary */
    EV_CALC_FITNESS_AT(ev, start + 2, evt->opt);
    
    EV_COPY_GREEDY_COUNT(ev, start + 1, start + 2, evt->opt, evt->improovs);

    /**
     * print status informations if wanted
     */
    if (ev->verbose >= EV_VERBOSE_ONELINE) {
      EV_IV_GREEDY_STATUS_OUTPUT(*ev, j);

      if (ev->verbose >= EV_VERBOSE_ULTRA)
        EV_THREAD_SAVE_NEW_LINE;
    }
  }

  /* set greedy best if generation best is better */
  EV_COPY_GREEDY(ev, start, start + 1, evt->opt);

  return NULL;
}

/**
 * returns the Size an Evolution with the given args will have
 */
uint64_t ev_size(int population_size, 
                 int num_threads, 
                 int keep_last_generation, 
                 uint64_t sizeof_iv, 
                 uint64_t sizeof_opt) {
  
  uint64_t mul = keep_last_generation ? 1 : 2;

  uint64_t size = (uint64_t) sizeof(Evolution);
  size += (uint64_t) sizeof(EvThreadArgs) * num_threads;
  size += (uint64_t) sizeof(TClient)      * num_threads;
  size += (uint64_t) sizeof(rand128_t *)  * num_threads;
  size += (uint64_t) sizeof(rand128_t)    * num_threads;
  size += sizeof_opt * num_threads;
  size += (uint64_t) sizeof(Individual *) * population_size * mul;
  size += (uint64_t) sizeof(Individual)   * population_size * mul;
  size += sizeof_iv  * population_size    * mul;

  return size;
}

/**
 * Initializes Thread Clients and Individuals serialized
 */
static void ev_init_tc_and_ivs_serial(Evolution *ev) {
 
  int i;

  /**
   * multiplicator: if we should discard the last generation, 
   * we can't recombinate in place
   */
  int mul = ev->keep_last_generation ? 1 : 2;
  mul = (ev->use_greedy ? 1 : mul);

  /* start and end of calculation */
  ev->overall_start = 0;
  ev->overall_end   = ev->population_size * mul;


  /**
   * Loop untill all individuals of this thread are initialized
   */
  for (i = 0; i < ev->overall_end; i++) {
     
    /**
     * create new individual
     */
    ev->population[i]     = ev->ivs + i;
    ev->population[i]->iv = ev->init_iv(*ev->opts);
    EV_CALC_FITNESS_AT(ev, i, *ev->opts);

    /**
     * prints status informations if wanted
     */
    if (ev->verbose >= EV_VERBOSE_ONELINE) {
      printf("init population: %10d\r", i);

      if (ev->verbose >= EV_VERBOSE_HIGH)
        printf("\n");
    }
  }

  /**
   * Select the best individual to survive,
   * Sort the Individuals by their fittnes
   */
  if (!ev->use_greedy)
    EV_SELECTION(ev); 

  if (ev->verbose >= EV_VERBOSE_HIGH)
    printf("Population Initialized\n");

}

/**
 * Initializes Thread Clients and Individuals serialized
 */
static void ev_init_tc_and_ivs_greedy_serial(Evolution *ev) {
 
  int i;

  /* start and end of calculation */
  ev->overall_start = 0;
  ev->overall_end   = ev->population_size;

  /**
   * create new individual s
   */
  for (i = 0; i < 3; i++) {
    ev->population[i]     = ev->ivs + i;
    ev->population[i]->iv = ev->init_iv(*ev->opts);
    EV_CALC_FITNESS_AT(ev, i, *ev->opts);
  }

  /**
   * generate random individuals and choose the best 
   */
  for (i = 0; i < ev->greedy_individuals; i++) {
     
    /**
     * create new individual
     */
    ev->free_iv(ev->population[1]->iv, *ev->opts);
    ev->population[1]->iv = ev->init_iv(*ev->opts);
    EV_CALC_FITNESS_AT(ev, 1, *ev->opts);

    /* set best individual if neccesary */
    EV_COPY_GREEDY_COUNT(ev, 0, 1, *ev->opts, ev->info.improovs);

    /**
     * prints status informations if wanted
     */
    if (ev->verbose >= EV_VERBOSE_ONELINE) {
      EV_INIT_IV_OUTPUT(i);

      if (ev->verbose >= EV_VERBOSE_HIGH)
        EV_THREAD_SAVE_NEW_LINE;
    }
  }

  if (ev->verbose >= EV_VERBOSE_HIGH)
    printf("Population Initialized\n");

}


/**
 * progress on generation on the ivs
 */
static inline void evolute_ivs_seriel(Evolution *ev) {
  
  if (ev->use_recombination)
    seriel_recombinate(ev);
  else if (ev->deaths == ev->survivors)
    seriel_mutation_onely_1half(ev);
  else
    seriel_mutation_onely_rand(ev);

}

/**
 * progress on generation on the ivs
 */
static inline void greedy_ivs_seriel(Evolution *ev) {
  
  int j;

  /* reset improovs */
  ev->info.improovs = 0;

  /* initialize generation best to greedy best */
  ev->clone_iv(ev->population[1]->iv, ev->population[0]->iv, *ev->opts);
  ev->population[1]->fitness = ev->population[0]->fitness;

  for (j = 0; j < ev->greedy_size; j++) {

    /* copy greedy best and mutate it */
    ev->clone_iv(ev->population[2]->iv, ev->population[0]->iv, *ev->opts);
    ev->mutate(ev->population[2], *ev->opts);

    /* calculate fitness and set generation best if neccesary */
    EV_CALC_FITNESS_AT(ev, 2, *ev->opts);
    
    EV_COPY_GREEDY_COUNT(ev, 1, 2, *ev->opts, ev->info.improovs);
  }

  /* set greedy best if generation best is better */
  EV_COPY_GREEDY(ev, 0, 1, *ev->opts);
}

/**
 * seriel recombinate
 */
static void seriel_recombinate(Evolution *ev) {

  int j, rand1, rand2;

  /**
   * for recombination there musst be min two individuals
   */
  #ifdef DEBUG
  if (ev->survivors <= 1)
    ERR_MSG("less than 2 individuals in recombinate");
  #endif

  /* reset improovs */
  ev->info.improovs = 0;  

  /**
   * loop untill all recombinations of this thread are done
   */
  for (j = ev->overall_start; j < ev->overall_end; j++) {

    /**
     * from two randomly choosen Individuals 
     * of the untouched (best) part we calculate an new one 
     * */
    rand2 = rand1 = rand128(ev->rands[0]) % ev->overall_start;
    while (rand1 == rand2) rand2 = rand128(ev->rands[0]) % ev->overall_start; 
    
    /* recombinate individuals */
    ev->recombinate(ev->population[rand1], 
                    ev->population[rand2], 
                    ev->population[j], 
                    *ev->opts);
    
    /* mutate Individuals */
    if (ev->use_muttation) {
      if (ev->always_mutate)
        ev->mutate(ev->population[j], *ev->opts);
      else {
        if (rand128(ev->rands[0]) <= ev->i_mut_propability)
          ev->mutate(ev->population[j], *ev->opts);
      }
    }

    /* calculate the fittnes for the new individuals */
    EV_CALC_FITNESS_AT(ev, j, *ev->opts);

    /**
     * store if the new individual is better as the old one
     */
    if (ev->sort_max) {
      if (ev->population[j]->fitness > ev->population[rand1]->fitness && 
          ev->population[j]->fitness > ev->population[rand2]->fitness) {

        ev->info.improovs++;
      }

    } else {
      if (ev->population[j]->fitness < ev->population[rand1]->fitness && 
          ev->population[j]->fitness < ev->population[rand2]->fitness) {

        ev->info.improovs++;
      }
    }

    /**
     * print status informations if wanted
     */
    if (ev->verbose >= EV_VERBOSE_ONELINE) {
      EV_IV_STATUS_OUTPUT_SERIEL(*ev, j);

      if (ev->verbose >= EV_VERBOSE_ULTRA)
        printf("\n");
    }
  }
}

/**
 * Thread function to do mutation onely
 * if number of survivors == number of deaths
 */
static void seriel_mutation_onely_1half(Evolution *ev) {

  int j;
  
  /* reset threadwide iprooves */
  ev->info.improovs = 0;  
 
  /**
   * loop untill all mutations of this thread are done
   */
  for (j = ev->overall_start; j < ev->overall_end; j++) {
 
    /**
     * clone the current individual (from the survivors)
     * and override an individual in the deaths-part
     */
    ev->clone_iv(ev->population[j]->iv, 
                 ev->population[j - ev->overall_start]->iv, 
                 *ev->opts);
 
    /* muttate the cloned individual */
    ev->mutate(ev->population[j], 
               *ev->opts);
 
    /* calculate the fittnes for the new individual */
    EV_CALC_FITNESS_AT(ev, j, *ev->opts);
    
    /**
     * store if the new individual is better as the old one
     */
    if (ev->sort_max) {
      if (EV_FITNESS_AT(ev, j) > 
          EV_FITNESS_AT(ev, j - ev->overall_start)) {
 
        ev->info.improovs++;
      }
 
    } else {
      if (EV_FITNESS_AT(ev, j) <
          EV_FITNESS_AT(ev, j - ev->overall_start)) {
 
        ev->info.improovs++;
      }
    }
    
    /**
     * print status informations if wanted
     */
    if (ev->verbose >= EV_VERBOSE_ONELINE) {
      EV_IV_STATUS_OUTPUT_SERIEL(*ev, j);

      if (ev->verbose >= EV_VERBOSE_ULTRA)
        printf("\n");
    }
  }
}

/**
 * Thread function to do mutation onely
 * if numbers of survivors != number of deaths
 */
static void seriel_mutation_onely_rand(Evolution *ev) {

  int j, rand1;

  /* reset threadwide iprooves */
  ev->info.improovs = 0;  
 
  /**
   * loop untill all mutations are done
   */
  for (j = ev->overall_start; j < ev->overall_end; j++) {
 
    /**
     * clone random individual (from the survivors)
     * and override the current individual in the deaths-part
     */
    rand1 = rand128(ev->rands[0]) % ev->overall_start;
    ev->clone_iv(ev->population[j]->iv, 
                 ev->population[rand1]->iv, 
                 *ev->opts);
 
    /* muttate the cloned individual */
    ev->mutate(ev->population[j], *ev->opts);
 
    /* calculate the fittnes for the new individual */
    EV_CALC_FITNESS_AT(ev, j, *ev->opts);
   
    /**
     * store if the new individual is better as the old one
     */
    if (ev->sort_max) {
      if (ev->population[j]->fitness > ev->population[rand1]->fitness) {
 
        ev->info.improovs++;
      }
 
    } else {
      if (ev->population[j]->fitness < ev->population[rand1]->fitness) {
 
        ev->info.improovs++;
      }
    }
   
    /**
     * print status informations if wanted
     */
    if (ev->verbose >= EV_VERBOSE_ONELINE) {
      EV_IV_STATUS_OUTPUT_SERIEL(*ev, j);

      if (ev->verbose >= EV_VERBOSE_ULTRA)
        printf("\n");
    }
  }
}

/**
 * prints informations about an given evolution
 */
void ev_inspect(Evolution *ev) {
  
  printf("Evolution:\n\t"
         "population_size:       %d\n\t"
         "generation_limit:      %d\n\t"
         "mutation_propability:  %f\n\t"
         "death_percentage:      %f\n\t"
         "use_recombination:     %d\n\t"
         "use_muttation:         %d\n\t"
         "always_mutate:         %d\n\t"
         "keep_last_generation:  %d\n\t"
         "use_abort_requirement: %d\n\t"
         "use_greedy:            %d\n\t"
         "deaths:                %d\n\t"
         "survivors:             %d\n\t"
         "sort_max:              %d\n\t"
         "verbose:               %d\n\t"
         "min_quicksort:         %d\n\t"
         "num_threads:           %d\n\t"
         "overall_start:         %d\n\t"
         "overall_end:           %d\n\t"
         "i_mut_propability:     %d\n\t",
         ev->population_size,
         ev->generation_limit,
         ev->mutation_propability,
         ev->death_percentage,
         ev->use_recombination,
         ev->use_muttation,
         ev->always_mutate,
         ev->keep_last_generation,
         ev->use_abort_requirement,
         ev->use_greedy,
         ev->deaths,
         ev->survivors,
         ev->sort_max,
         ev->verbose,
         ev->min_quicksort,
         ev->num_threads,
         ev->overall_start,
         ev->overall_end,
         ev->i_mut_propability);
}

#endif /* end of EVOLUTION */
