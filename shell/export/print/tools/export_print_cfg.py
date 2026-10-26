ORI={'TriggerPost':'-Z up (flipped)','TopShell':'+Z up (as modelled)','BottomShell':'-Z up (flipped)','BatteryPanel':'-Y up','BumperCap':'-Z up (flipped)','Trigger':'-X up'}
for n in ['HallBridge','PowerPlate','SidePlateL1','SidePlateL2','SidePlateR1','SidePlateR2']+['FaceCap%d'%i for i in range(1,7)]: ORI[n]='+Z up (as modelled)'
