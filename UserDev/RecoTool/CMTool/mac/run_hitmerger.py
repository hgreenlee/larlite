import sys,os
# from ROOT import larlite as fmwk
# from recotool.mergeDef import *

from larlite import larlite as fmwk

mgr = fmwk.ana_processor()

#args should be input file name
for x in xrange(len(sys.argv)-2):

    mgr.add_input_file(sys.argv[x+1])

#last arg should be output file name
out_file = sys.argv[-1]
if os.path.isfile(out_file):
    print
    print 'ERROR: output file already exist...'
    print
    sys.exit(0)

mgr.set_output_file(out_file)

mgr.set_io_mode(fmwk.storage_manager.kBOTH)

mgr.set_ana_output_file("")

prelim = fmwk.HitCluster()
# prelim.SetInputProducer("gaushit")
prelim.SetInputProducer("pandoraCosmicKHitRemoval")
prelim.SetInputVertexProducer("pandoraNu")
prelim.SetInputSpacePointProducer("pandoraNu")
# prelim.SetOutputProducer("Mergedgaushit")
prelim.SetOutputProducer("pandoraCosmicKHitRemoval")
mgr.add_process(prelim)


mgr.run()


