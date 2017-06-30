//base class for matrix Free implementation of PDE's
#ifndef MATRIXFREEPDE_H
#define MATRIXFREEPDE_H

//general headers
#include <fstream>
#include <sstream>
#include <iterator>

//dealii headers
#include "dealIIheaders.h"

//PRISMS headers
#include "varBCs.h"
#include "fields.h"
#include "userInputParameters.h"
#include "nucleus.h"
#include "variableValueContainer.h"
#include "variableContainer.h"
#include "residualContainer.h"


////define data types
#ifndef scalarType
typedef dealii::VectorizedArray<double> scalarType;
#endif
#ifndef vectorType
typedef dealii::parallel::distributed::Vector<double> vectorType;
#endif

//macro for constants
#define constV(a) make_vectorized_array(a)

//
using namespace dealii;
//
//base class for matrix free PDE's
//
/**
 * This is the abstract base class for the matrix free implementation of Parabolic and Elliptic BVP's,
 * and supports MPI, Threads and Vectorization (Hybrid Parallel).
 * This class contains the parallel data structures, mesh (referred to as triangulation),
 * parallel degrees of freedom distribution,  constraints,  and general utility methods.
 *
 * All the physical models in this package inherit this base class.
 */
template <int dim, int degree>
class MatrixFreePDE:public Subscriptor
{
 public:
  /**
   * Class contructor
   */
  MatrixFreePDE(userInputParameters<dim>);
  ~MatrixFreePDE();
  /**
   * Initializes the mesh, degrees of freedom, constraints and data structures using the user provided
   * inputs in the application parameters file.
   */
  void init  ();

   /**
   * Initializes the data structures for enabling unit tests.
   *
   * This method initializes the MatrixFreePDE object with a fixed geometry, discretization and
   * other custom selected options specifically to help with unit tests, and should not be called
   * in any of the physical models.
   */
  void initForTests();

  /**
   * This method implements the time stepping algorithm and invokes the solveIncrement() method.
   */
  void solve ();
  /**
   * This method essentially converts the MatrixFreePDE object into a matrix object which can be
   * used with matrix free iterative solvers. Provides the A*x functionality for solving the system of
   * equations AX=b.
   */
  void vmult (vectorType &dst, const vectorType &src) const;
  /**
   * Vector of all the physical fields in the problem. Fields are identified by dimentionality (SCALAR/VECTOR),
   * the kind of PDE (ELLIPTIC/PARABOLIC) used to compute them and a character identifier  (e.g.: "c" for composition)
   * which is used to write the fields to the output files.
   */
  std::vector<Field<dim> >                  fields;

  void buildFields();

  // Parallel message stream
  ConditionalOStream  pcout;

 protected:
  const userInputParameters<dim> userInputs;

  dealii::Threads::Mutex assembler_lock;

  unsigned int totalDOFs;

  // Elasticity matrix variables
  const static unsigned int CIJ_tensor_size = 2*dim-1+dim/3;

  // Method to reinitialize the mesh, degrees of freedom, constraints and data structures when the mesh is adapted
  void reinit  ();

  /**
   * Method to solve each time increment of a time-dependent problem. For time-independent problems
   * this method is called only once. This method solves for all the fields in a staggered manner (one after another)
   * and also invokes the corresponding solvers: Explicit solver for Parabolic problems, Implicit (matrix-free) solver for Elliptic problems.
   */
  void solveIncrement ();
  /* Method to write solution fields to vtu and pvtu (parallel) files.
  *
  * This method can be enabled/disabled by setting the flag writeOutput to true/false. Also,
  * the user can select how often the solution files are written by setting the flag
  * skipOutputSteps in the parameters file.
  */
  void outputResults() const;

  /*Parallel mesh object which holds information about the FE nodes, elements and parallel domain decomposition
   */
  parallel::distributed::Triangulation<dim> triangulation;
  /*A vector of finite element objects used in a model. For problems with only one primal field,
   *the size of this vector is one,otherwise the size is the number of primal fields in the problem.
  */
  std::vector<FESystem<dim>*>          FESet;
  /*A vector of all the constraint sets in the problem. A constraint set is a map which holds the mapping between the degrees
   *of freedom and the corresponding degree of freedom constraints. Currently the type of constraints stored are either
   *Dirichlet boundary conditions or hanging node constraints for adaptive meshes.
   */
  std::vector<const ConstraintMatrix*> constraintsDirichletSet, constraintsOtherSet;
  /*A vector of all the degree of freedom objects is the problem. A degree of freedom object handles the serial/parallel distribution
   *of the degrees of freedom for all the primal fields in the problem.*/
  std::vector<const DoFHandler<dim>*>  dofHandlersSet;

  /*A vector of the locally relevant degrees of freedom. Locally relevant degrees of freedom in a parallel implementation is a collection of the
   *degrees of freedom owned by the current processor and the surrounding ghost nodes which are required for the field computations in this processor.
   */
  std::vector<const IndexSet*>         locally_relevant_dofsSet;
  /*Copies of constraintSet elements, but stored as non-const to enable application of constraints.*/
  std::vector<ConstraintMatrix*>       constraintsDirichletSet_nonconst, constraintsOtherSet_nonconst;
  /*Copies of dofHandlerSet elements, but stored as non-const.*/
  std::vector<DoFHandler<dim>*>        dofHandlersSet_nonconst;
  /*Copies of locally_relevant_dofsSet elements, but stored as non-const.*/
  std::vector<IndexSet*>               locally_relevant_dofsSet_nonconst;
  /*Vector all the solution vectors in the problem. In a multi-field problem, each primal field has a solution vector associated with it.*/
  std::vector<vectorType*>             solutionSet;
  /*Vector all the residual (RHS) vectors in the problem. In a multi-field problem, each primal field has a residual vector associated with it.*/
  std::vector<vectorType*>             residualSet;
  /*Vector of parallel solution transfer objects. This is used only when adaptive meshing is enabled.*/
  std::vector<parallel::distributed::SolutionTransfer<dim, vectorType>*> soltransSet;

  // Objects for vectors
  DoFHandler<dim>* vector_dofHandler;
  FESystem<dim>* vector_fe;
  MatrixFree<dim,double> vector_matrixFreeObject;

  //matrix free objects
   /*Object of class MatrixFree<dim>. This is primarily responsible for all the base matrix free functionality of this MatrixFreePDE<dim> class.
   *Refer to deal.ii documentation of MatrixFree<dim> class for details.
   */
  MatrixFree<dim,double>               matrixFreeObject;
  /*Vector to store the inverse of the mass matrix diagonal. Due to the choice of spectral elements with Guass-Lobatto quadrature, the mass matrix is diagonal.*/
  vectorType                           invM;
  /*Vector to store the solution increment. This is a temporary vector used during implicit solves of the Elliptic fields.*/
  vectorType                           dU_vector, dU_scalar;

  //matrix free methods
  /*Current field index*/
  unsigned int currentFieldIndex;
  /*Method to compute the inverse of the mass matrix*/
  void computeInvM();


  /*AMR methods*/
  void refineGrid();
  /*Virtual method to mark the regions to be adaptively refined. This is expected to be provided by the user.*/
  void adaptiveRefine(unsigned int _currentIncrement);
  /*Virtual method to define AMR refinement criterion. The default implementation uses the Kelly error estimate for estimative the error function. The user can supply a custom implementation to overload the default implementation.*/
  virtual void adaptiveRefineCriterion();

  /*Method to compute the right hand side (RHS) residual vectors*/
  void computeRHS();

  //virtual methods to be implemented in the derived class
  /*Method to calculate LHS(implicit solve)*/
  void getLHS(const MatrixFree<dim,double> &data,
		      vectorType &dst,
		      const vectorType &src,
		      const std::pair<unsigned int,unsigned int> &cell_range) const;
  /*Method to calculate RHS (implicit/explicit). This is an abstract method, so every model which inherits MatrixFreePDE<dim> has to implement this method.*/
  void getRHS (const MatrixFree<dim,double> &data,
		       std::vector<vectorType*> &dst,
		       const std::vector<vectorType*> &src,
		       const std::pair<unsigned int,unsigned int> &cell_range) const;

  // virtual void residualRHS(const std::vector<modelVariable<dim> > & modelVarList,
  // 		  	  	  	  	  	  	  	  	  	  	  	  	  std::vector<modelResidual<dim> > & modelResidualsList,
  // 														  dealii::Point<dim, dealii::VectorizedArray<double> > q_point_loc) const=0;
  virtual void residualRHS(const variableContainer<dim,dealii::VectorizedArray<double> > & variable_list,
  		  	  	  	  	  	  	  	  	  	  	  	  	  residualContainer<dim,dealii::VectorizedArray<double> > & residual_list,
  														  dealii::Point<dim, dealii::VectorizedArray<double> > q_point_loc) const=0;

  virtual void residualLHS(const std::vector<modelVariable<dim> > & modelVarList,
    		  	  	  	  	  	  	  	  	  	  	  	  	  modelResidual<dim> & modelRes,
  														  dealii::Point<dim, dealii::VectorizedArray<double> > q_point_loc) const=0;

  virtual void energyDensity(const std::vector<modelVariable<dim> > & modelVarList, const dealii::VectorizedArray<double> & JxW_value,
  		  	  	  	  	  	  	  	  	  	  	  	  	  dealii::Point<dim, dealii::VectorizedArray<double> > q_point_loc)=0;


  virtual void postProcessedFields(const std::vector<modelVariable<dim> > & modelVariablesList,
                                                              std::vector<modelResidual<dim> > & modelResidualsList,
                                                              const dealii::Point<dim, dealii::VectorizedArray<double> > q_point_loc) const {};
  void computePostProcessedFields(std::vector<vectorType*> &postProcessedSet) const;

  void getPostProcessedFields(const dealii::MatrixFree<dim,double> &data,
                                                                                      std::vector<vectorType*> &dst,
                                                                                      const std::vector<vectorType*> &src,
                                                                                      const std::pair<unsigned int,unsigned int> &cell_range) const;

  //methods to apply dirichlet BC's
  /*Map of degrees of freedom to the corresponding Dirichlet boundary conditions, if any.*/
  std::vector<std::map<dealii::types::global_dof_index, double>*> valuesDirichletSet;
  /*Virtual method to mark the boundaries for applying Dirichlet boundary conditions.  This is usually expected to be provided by the user.*/
  void markBoundaries();
  /*Virtual method for applying Dirichlet boundary conditions.  This is usually expected to be provided by the user.*/
  void applyDirichletBCs();

  // Methods to apply periodic BCs
  void setPeriodicity();
  void setPeriodicityConstraints(ConstraintMatrix *, const DoFHandler<dim>*) const;
  void getComponentsWithRigidBodyModes(std::vector<int> &) const;
  void setRigidBodyModeConstraints(const std::vector<int>, ConstraintMatrix *, const DoFHandler<dim>*) const;

  //methods to apply initial conditions
  /*Virtual method to apply initial conditions.  This is usually expected to be provided by the user in IBVP (Initial Boundary Value Problems).*/

  void applyInitialConditions();

  // --------------------------------------------------------------------------
  // Nucleation methods and variables
  // --------------------------------------------------------------------------
  // Vector of all the nuclei seeded in the problem
  std::vector<nucleus<dim> > nuclei;

  // Method to get a list of new nuclei to be seeded
  void updateNucleiList();
  std::vector<nucleus<dim> > getNewNuclei();
  void getLocalNucleiList(std::vector<nucleus<dim> > & newnuclei) const;
  void safetyCheckNewNuclei(std::vector<nucleus<dim> > newnuclei, std::vector<unsigned int> & conflict_ids);
  void refineMeshNearNuclei(std::vector<nucleus<dim> > newnuclei);

  // Method to obtain the nucleation probability for an element, nontrival case must be implemented in the subsclass
  virtual double getNucleationProbability(variableValueContainer, double)const {return 0.0;};

  /*Method to compute energy like quantities.*/
  void computeEnergy();
  void getEnergy(const MatrixFree<dim,double> &data,
		    std::vector<vectorType*> &dst,
		    const std::vector<vectorType*> &src,
		    const std::pair<unsigned int,unsigned int> &cell_range);

  //utility functions
  /*Returns index of given field name if exists, else throw error.*/
  unsigned int getFieldIndex(std::string _name);

  std::vector<double> freeEnergyValues;
  void outputFreeEnergy(const std::vector<double>& freeEnergyValues) const;

  /*Method to compute the integral of a field.*/
  void computeIntegral(double& integratedField, int index);

  //variables for time dependent problems
  /*Flag used to see if invM, time stepping in run(), etc are necessary*/
  bool isTimeDependentBVP;
  /*Flag used to mark problems with Elliptic fields.*/
  bool isEllipticBVP;
  //
  unsigned int parabolicFieldIndex, ellipticFieldIndex;
  double currentTime;
  unsigned int currentIncrement, currentOutput;

  /*Timer and logging object*/
  mutable TimerOutput computing_timer;

  double energy;
  std::vector<double> energy_components;

};

#endif
