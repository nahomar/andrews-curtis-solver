from setuptools import Extension, find_packages, setup

setup(
    name='ac-shortcuts',
    version='0.1.0',
    description='Goal-conditioned, fully charged Stable AC waypoint shortening',
    license='MIT',
    license_files=['LICENSE'],
    python_requires='>=3.9',
    packages=find_packages(),
    ext_modules=[Extension('ac_shortcuts._acc_word_kernels',
                           ['ac_shortcuts/native_word_kernels.cpp'],
                           extra_compile_args=['-O3', '-std=c++17'],
                           language='c++', optional=True)],
    entry_points={'console_scripts': ['ac-shortcut=ac_shortcuts.__main__:main']},
)
