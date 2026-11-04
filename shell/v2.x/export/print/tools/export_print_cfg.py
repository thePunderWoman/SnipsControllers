"""Print orientation per part (v2). Each entry is a list of (axis, degrees) rotations applied to the modelled part,
then the part is dropped onto Z=0 and centred on XY. Shared by export_print.py (STL) and step_orient.py (STEP)."""
ORI = {
    # name: (rotations, orientation note)
    'TopShell':     ([], '+Z up (as modelled): seam rim on the bed'),
    'BottomShell':  ([], '+Z up (as modelled): belly on the bed, open side up (scoop needs supports)'),
    'BatteryPanel': ([], '+Z up (as modelled): outer face on the bed'),
    'Bumper':       ([('x', -90)], 'front face on the bed'),
    'PowerButton':  ([('x', -90)], 'flange on the bed, stem up'),
    'SideButtonL1': ([('y', 90)], 'flat base on the bed, D stem up'),
    'SideButtonL2': ([('y', 90)], 'flat base on the bed, D stem up'),
    'SideButtonR1': ([('y', -90)], 'flat base on the bed, D stem up'),
    'SideButtonR2': ([('y', -90)], 'flat base on the bed, D stem up'),
    'Trigger':      ([('y', -90)], 'big flat blade face on the bed, spring ear up'),
    'TriggerPost':  ([('x', 180)], 'D flat on the bed, post lying along X'),
}
for _i in range(1, 7):
    ORI['FaceButton%d' % _i] = ([], '+Z up (as modelled): flange on the bed, stem and cap up')
# TriggerCaps holds two thin plates (left and right). Each lies flat; they are laid out side by side.
CAPS_ROT = [('y', -90)]
CAPS_GAP = 4.0          # mm between the two plates on the bed
