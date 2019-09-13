#!/usr/bin/env python

import argparse
import os
import re
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser(description='Draw Net')
parser.add_argument('input_file_names', nargs='+')
parser.add_argument('-d', '--m1_direction', default='horizontal')
args = parser.parse_args()


class Box:
    def __init__(self, layer, lx, hx, ly, hy):
        self.layer = int(layer)
        self.lx = int(lx)
        self.hx = int(hx)
        self.ly = int(ly)
        self.hy = int(hy)

    def __repr__(self):
        return 'box(l={}, x=({}, {}), y=({}, {}))'.format(self.layer, self.lx, self.hx, self.ly, self.hy)

    def w(self):
        return self.hx - self.lx

    def h(self):
        return self.hy - self.ly


for file_name in args.input_file_names:
    pinNames = []
    pinAccessBoxes = []
    routeGuides = []

    reHeader = re.compile('Net (.*) \(idx = (\d+)\) with (\d+) pins')
    rePin = re.compile('pin (.*)')
    reGuide = re.compile('(\d+) route guides')
    reBox = re.compile('box\(l=(\d+), x=\((-?\d+), (-?\d+)\), y=\((-?\d+), (-?\d+)\)\)')
    with open(file_name) as file:
        result = None
        while not result:
            line = file.readline()
            result = reHeader.search(line)
            netName, netIdx, numPins = result.group(1), int(result.group(2)), int(result.group(3))

        # pin
        line = file.readline()
        for _ in range(numPins):
            result = rePin.search(line)
            pinNames.append(result.group(1))
            boxes = []
            while True:
                line = file.readline()
                result = reBox.search(line)
                if result is None:
                    break
                boxes.append(Box(result.group(1), result.group(2), result.group(3), result.group(4), result.group(5)))
            pinAccessBoxes.append(boxes)

        # guide
        numGuides = int(reGuide.search(line).group(1))
        for _ in range(numGuides):
            result = reBox.search(file.readline())
            routeGuides.append(Box(result.group(1), result.group(2), result.group(3), result.group(4), result.group(5)))

    # num of layers
    numLayers = 0
    for accessBoxes in pinAccessBoxes:
        for box in accessBoxes:
            numLayers = max(box.layer, numLayers)
    for box in routeGuides:
        numLayers = max(box.layer, numLayers)
    numLayers += 1
    print('# layers is {}'.format(numLayers))

    # define box plotting
    cmap = plt.get_cmap('plasma')
    colors = [cmap(i / (numLayers - 1)) for i in range(0, numLayers)]
    hatches = ['--', '||', '//', '\\', '++', 'xx', 'oo', 'OO', '..', '**']
    horiOffset = 0 if args.m1_direction == 'horizontal' else 1
    labels = ['M{} '.format(i+1) + ('horizontal' if i % 2 == horiOffset else "vertical") for i in range(0, numLayers)]
    def plotBox(box):
        plt.gca().add_patch(plt.Rectangle((box.lx, box.ly), box.w(), box.h(), lw=1,
                                        edgecolor='k', facecolor=colors[box.layer], alpha=0.3, hatch=hatches[box.layer], label=labels[box.layer]))


    # plot boxes (in the order of layers to make legend ordered)
    plt.figure(figsize=(8, 8))
    plt.axes()
    boxes = [[] for _ in range(numLayers)]
    for accessBoxes in pinAccessBoxes:
        for box in accessBoxes:
            boxes[box.layer].append(box)
    for box in routeGuides:
        boxes[box.layer].append(box)
    for (i, boxesSameLayer) in enumerate(boxes):
        if len(boxesSameLayer) == 0:
            continue
        for box in boxesSameLayer:
            plotBox(box)
    plt.legend()

    # anno pins
    for pinIdx in range(numPins):
        pinX = 0
        pinY = 0
        layerSet = set()
        for box in pinAccessBoxes[pinIdx]:
            pinX += box.lx + box.w() / 2
            pinY += box.ly + box.w() / 2
            layerSet.add(box.layer)
        layerList = list(layerSet)
        layerList.sort()
        anno = 'pin {} '.format(pinNames[pinIdx])
        for layer in layerList:
            anno += 'M{}'.format(layer+1)
        plt.text(pinX / len(pinAccessBoxes[pinIdx]), pinY / len(pinAccessBoxes[pinIdx]), anno, color='k')

    # format
    plt.xlabel('X (DBU)')
    plt.ylabel('Y (DBU)')
    plt.title('{} with {} pins and {} route guides'.format(netName, len(pinAccessBoxes), len(routeGuides)))
    plt.axis('square')
    plt.axis('scaled')

    # save
    base_name, ext_name = os.path.splitext(os.path.basename(file_name))
    plt.savefig('{}.pdf'.format(base_name), bbox_inches='tight')
    plt.savefig('{}.png'.format(base_name), bbox_inches='tight')
    # plt.show()