ORI={'TriggerPost':'-Z up (flipped)','TopShell':'+Z up (as modelled)','BottomShell':'-Z up (flipped)','BatteryPanel':'+Z up (as modelled)','BumperCap':'+Y up','Trigger':'+X down (rot Y 90)'}
for n in ['HallBridge','PowerPlate']+['FaceCap%d'%i for i in range(1,7)]: ORI[n]='+Z up (as modelled)'
ORI['PowerPlate']='+Y up'
ORI.update({'SidePlateL1':'+X up','SidePlateL2':'+X up','SidePlateR1':'-X up','SidePlateR2':'-X up'})
