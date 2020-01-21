import sys

if len(sys.argv) < 2:
    msg  = '\n'
    msg += "Usage 1: %s $INPUT_ROOT_FILE\n" % sys.argv[0]
    msg += '\n'
    sys.stderr.write(msg)
    sys.exit(1)


# Import the needed modules.  Need larlite and several of it's namespaces
from ROOT import gSystem,TMath
from larlite import larlite as fmwk
from larlite import larutil
from recotool import cmtool, showerreco

from ROOT import calo
from recotool.matchDef import * 

# usage:
# > python matchAndShowerRecoUboone.py cluster_file.root cluster_producer

def getShowerRecoAlgModular():
  # This function returns the default shower reco module
  # If you want to extend, customize, or otherwise alter the default
  # reco (which you should!) it's recommended that you do one of two
  # things:
  #   1) Copy this function, and change the lines you need
  #   2) Use this function as is and replace the modules with the built-in functions
  #       to ShowerRecoAlgModular
  # Then, use the return value (alg) in place of ShowerRecoAlg in your python script

  # It should be noted, however, that this "default" set of modules is not yet developed
  # and if you are developing you ought to be updating it here!

  alg = showerreco.ShowerRecoAlgModular()
  alg.SetDebug(False)

  # 3D Axis Module:
  axis3D = showerreco.Axis3DModule()
  axis3D.setMaxIterations(100)
  axis3D.setNStepsInitial(25)
  axis3D.setTargetError(0.001)
  axis3D.setNormalErrorRange(0.01)
  axis3D.setThetaRangeStart(0.1)
  axis3D.setThetaRangeMin(0.0005)
  axis3D.setNStepsStart(4)
  axis3D.setConvergeRate(0.85)
  axis3D.setVerbosity(True)
  axis3D.setSeedVectorErrorCutoff(0.1)

  angle3D = showerreco.Angle3DFormula()
  angle3D.setMaxAngleError(0.1)
  angle3D.setVerbosity(False)

  energy = showerreco.LinearEnergy()
  energy.SetUseModBox(True)
  energy.setVerbosity(False)

  dqdx = showerreco.dQdxModule()

  dedx = showerreco.dEdxFromdQdx()
  dedx.SetUsePitch(False)
  dedx.setVerbosity(False)

  #alg.AddShowerRecoModule(axis3D)
  alg.AddShowerRecoModule(angle3D)
  alg.AddShowerRecoModule(showerreco.StartPoint3DModule()  )
  alg.AddShowerRecoModule(energy)
  alg.AddShowerRecoModule(dqdx)
  alg.AddShowerRecoModule(dedx)
  # alg.AddShowerRecoModule(showerreco.StartPoint2DModule()  )
  #alg.AddShowerRecoModule(showerreco.OtherStartPoint3D()  )
  # alg.AddShowerRecoModule(showerreco.ShowerChargeModule()  )

  alg.AddShowerRecoModule(showerreco.GeoModule())

  alg.PrintModuleList()

  return alg

def DefaultShowerReco3D():

    # Create analysis unit
    ana_unit = fmwk.ShowerReco3D()
    
    # Attach shower reco alg
    sralg = getShowerRecoAlgModular()
    # sralg.SetDebug()
    # calg = calo.CalorimetryAlg()
    # sralg.CaloAlgo(calg)
    # sralg.SetUseModBox(True)
    ana_unit.AddShowerAlgo(sralg)

    return ana_unit


# Create ana_processor instance
my_proc = fmwk.ana_processor()

# Set input root file
for x in range(len(sys.argv)-2):
    my_proc.add_input_file(sys.argv[x+1])

# Specify IO mode
my_proc.set_io_mode(fmwk.storage_manager.kBOTH)

my_proc.set_ana_output_file("ana.root")
my_proc.set_output_file("showers.root")

# DefaultMatch is the function called from $LARLITE/python/recot...
palgo_array, algo_array = DefaultMatch()

match_maker = fmwk.ClusterMatcher()
match_producer = "MatchCluster"
cluster_producer = sys.argv[-1]

match_maker.GetManager().AddPriorityAlgo(palgo_array)
match_maker.GetManager().AddMatchAlgo(algo_array)

match_maker.SetClusterProducer(cluster_producer)
match_maker.SetOutputProducer(match_producer)
match_maker.SaveOutputCluster(True)

ana_unit=DefaultShowerReco3D()
ana_unit.SetInputProducer(match_producer) #,True)
ana_unit.SetOutputProducer("showerreco")

my_proc.add_process(match_maker)
my_proc.add_process(ana_unit)

print()
print("Finished configuring ana_processor. Start event loop!")
print()

my_proc.run()

sys.exit()

