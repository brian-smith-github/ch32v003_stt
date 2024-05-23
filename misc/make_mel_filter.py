import librosa
import numpy
#melmx = librosa.filters.mel(sr=8000, n_fft=512, n_mels=24, fmin=0,fmax=4000)
#melmx = librosa.filters.mel(sr=8000, n_fft=256, n_mels=32, fmin=0,fmax=4000)
melmx = librosa.filters.mel(sr=6400, n_fft=128, n_mels=20, fmin=0,fmax=3200)

numpy.savetxt("mel.txt",melmx, delimiter=",")
