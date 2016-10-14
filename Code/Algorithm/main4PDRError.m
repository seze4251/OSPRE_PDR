% Cameron Maywood, Anthony Torres, Seth Zegelstein
% Date Created: 9/27/2016
% % Date Modified:
% Purpose: Main Script for PDR
close all; clc;

% Camera 1 18 degree FOV, 13 Mega Pixels
clearvars;

pixels = []; %pixels
FOV = []; %degrees
plots = 3; % Show Plots
pixPerDeg = [ 232 232]; % Use pixel and FOV in calculations
plotStr{1} = sprintf('Position Error vs Distance for ZenFone Zoom Camera');
plotStr{2} = sprintf('Velocity Error vs Distance for ZenFone Zoom Camera');
pixel_error_vec = linspace(1,100,1000);
pixel_error_vec = linspace(.5,8,6);

[validSolution, minDegAccuracy] = PositionVelocityError( pixels, FOV, plots, pixPerDeg, plotStr, pixel_error_vec );

if validSolution == 0
    str = 'a valid solution.';
else
    str = 'a invalid solution.';
end

fprintf('This option is %s Image processing must be accurate to within %.5f degrees \n in order to meet requirments for top choice \n camera within volume constraints \n', str, minDegAccuracy);

% Create Maximum Image Error vs pixel/deg plot
 clearvars;
% % 
pixels = []; %pixels
FOV = []; %degrees
plots = 0; % Show Plots
plotStr = [];
pixPerDeg = linspace(1,250,40); % Use pixel and FOV in calculations
pixel_error_vec = linspace(1,1000,100);
count = 1;

for i = pixPerDeg
    [validSolution, minDegAccuracy(count)] = PositionVelocityError( pixels, FOV, plots, [i i], plotStr, pixel_error_vec );
    count = count + 1;
end

figure;
plot(pixPerDeg, minDegAccuracy)
xlabel 'Pixel/Degree'
ylabel 'Maximum Allowable Degree Error From Image Processing (Degree)'
tit = sprintf('Maximum Image processing Error vs Pixel/Degree \n required for a valid solution');
title(tit);

% Create Maximum Position/ Velocity Error vs Image Processing Error
clearvars;

count = 1;
sigma_eff = linspace(0.01,.06,50);
[r_error, v_error] = PosVelError2( sigma_eff );

for i = 1 : length(sigma_eff)
    MaxPosError(i) = max(r_error(i,:));
    MaxVelError(i) = max(v_error(i,:));
end

figure;
subplot(1,2,1)
hold on
plot(sigma_eff, MaxPosError)
plot(sigma_eff,1000*ones(length(sigma_eff)), '--')
xlabel 'Degree Error'
ylabel 'Maximum Position Error (km)'
ylim([0 1300])
tit = sprintf('Position Error vs Degree Error');
title(tit);
hold off

subplot(1,2,2)
hold on
plot(sigma_eff, MaxVelError)
plot(sigma_eff,250*ones(length(sigma_eff)), '--')
xlabel 'Degree Error'
ylabel 'Maximum Velocity Error (m/s)'
ylim([0 350])
tit = sprintf('Velocity Error vs Degree Error');
title(tit);
hold off

ind = find(MaxVelError > 250);
maxDegError = sigma_eff(ind(1)-1);
maxDegError
%% Relate Pixel / Degree to degree Error
 clc; clearvars -except maxDegError;


pixelperdeg = linspace(3,250,1000);
pixelerror = linspace(1 ,8,6 );

figure;
hold on
for i = 1 : length(pixelerror)
    sigma = 1 ./ pixelperdeg * pixelerror(i);
    plot(pixelperdeg, sigma)
    str{i} = sprintf('Pixel Error %1.2f', pixelerror(i));
end
vline([72 232],{'g--','r--','b'},{'Cell Phone Camera','ZenFone Camera'})
legend(str)
plot(pixelperdeg, maxDegError * ones(length(pixelperdeg)),'k--')
ylim([0 0.05])
xlabel 'Pixel Per Degree'
ylabel 'Degree Error (deg)'
tit = sprintf('Degree Error vs Pixel Per Degree \n for Various Amounts of Pixel Error');
title(tit)
hold off

%% Maximum Position Error to Pixel per Degree for different 
clearvars;
pixPerDeg = linspace(1,250,100); % Use pixel and FOV in calculations
pixel_error_vec = linspace(1,10,10);

% OPTION 2
pixel_error_vec = linspace(.5,8,6);

% Calculate R_Error and V_Error
count = 1;
for i = pixPerDeg
    [r_error{count}, v_error{count} ] = PositionVelocityError3( i, pixel_error_vec );
    count = count + 1;
end

% Get Plotting Vectors from r_error and v_error
for i = 1 : length(pixPerDeg)
    for j = 1 : length(pixel_error_vec)
        Max_Pos_Error (j,i) = max(r_error{i}(j,:));
        Max_Vel_Error (j,i) = max(v_error{i}(j,:));
    end
end

% Position Plot
figure;
hold on
for i = 1 : size(Max_Pos_Error,1)
    plot(pixPerDeg,Max_Pos_Error(i,:))
    str{i} = sprintf('Pixel Error %1.2f (Pixels)',pixel_error_vec(i));
end
legend(str);
vline([72 232],{'g--','r--','b'},{'Cell Phone Camera','ZenFone Camera'})
hline(1000,'k--','Maximum Position Error')
ylim([0 1500])
xlabel 'Pixel Per Degree'
ylabel 'Position Error (km)'
title 'Position Error vs Pixel Per Degree'

% Velocity Plot
figure;
hold on
for i = 1 : size(Max_Vel_Error,1)
    plot(pixPerDeg, Max_Vel_Error(i,:))
    str{i} = sprintf('Pixel Error %1.2f (Pixels)', pixel_error_vec(i));
end

legend(str);
vline([72 232],{'g--','r--','b'},{'Cell Phone Camera','ZenFone Camera'})
hline(250,'k--','Maximum Velocity Error')
ylim([0 350])
xlabel 'Pixel Per Degree'
ylabel 'Velocity Error (m/s)'
title 'Velocity Error vs Pixel Per Degree'


