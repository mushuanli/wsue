<B>docker 方式启动</B>
#!/bin/bash

DATADIR=/home/build/rain/tmp/tensorflow
IMGNAME=tensorflow/tensorflow



docker run  --name notebooks -d -v /$DATADIR/notebooks:/notebooks -v /$DATADIR/logs:/logs -p 8888:8888 $IMGNAME /run_jupyter.sh --allow-root --NotebookApp.token=''
docker run  --name board -d -v /$DATADIR/logs:/logs -p 6006:6006 $IMGNAME tensorboard --logdir /logs

