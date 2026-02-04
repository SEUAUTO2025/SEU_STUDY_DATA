%==========================================================================
% solution.m - Compares a custom Interior-Point Method vs. quadprog
%              for QP problems of varying sizes.
%==========================================================================
clear all;
clc;

% --- 1. Define Problem Sizes and Timing Configuration ---
problem_sizes = [10, 40, 100];
num_constraints_factor = 0.5; % n_constraints = n_vars * factor

% --- 2. Warm-up Solvers ---
% MATLAB's JIT can cause the first run to be slower. A warm-up run mitigates this.
fprintf('Warming up solvers (this may take a moment)...\n');
try
    warmup_problem = generateQP(10, 5);
    f_warmup_ipm = @() pathFollowingQP(warmup_problem.Q, warmup_problem.c, warmup_problem.A, warmup_problem.b, warmup_problem.x0, true);
    timeit(f_warmup_ipm);
    options_warmup = optimoptions('quadprog', 'Display', 'none', 'Algorithm', 'interior-point-convex');
    f_warmup_qp = @() quadprog(warmup_problem.Q, warmup_problem.c, warmup_problem.A, warmup_problem.b, [], [], [], [], [], options_warmup);
    timeit(f_warmup_qp);
    fprintf('Warm-up complete.\n\n');
catch ME
    fprintf('Warm-up failed, continuing anyway. Error: %s\n', ME.message);
end


% --- 3. Run Comparison Loop ---
fprintf('Starting QP Solver Comparison for Different Problem Sizes...\n\n');
results = cell(length(problem_sizes), 5);

for i = 1:length(problem_sizes)
    n = problem_sizes(i);
    m = floor(n * num_constraints_factor);
    fprintf('--- Generating and Solving Problem for n = %d variables, m = %d constraints ---\n', n, m);

    % Generate a new random, strictly feasible QP problem
    problem = generateQP(n, m);
    
    % --- Time custom Interior-Point Method using timeit for accuracy ---
    f_ipm = @() pathFollowingQP(problem.Q, problem.c, problem.A, problem.b, problem.x0, true);
    time_ipm = timeit(f_ipm);
    
    % --- Time MATLAB's quadprog (interior-point-convex) using timeit ---
    options = optimoptions('quadprog', 'Display', 'none', 'Algorithm', 'interior-point-convex');
    f_qp = @() quadprog(problem.Q, problem.c, problem.A, problem.b, [], [], [], [], [], options);
    time_quadprog = timeit(f_qp);

    % Get final solutions for comparison (run one last time outside of timeit)
    [x_ipm, ~] = pathFollowingQP(problem.Q, problem.c, problem.A, problem.b, problem.x0, true);
    [x_qp, ~] = quadprog(problem.Q, problem.c, problem.A, problem.b, [], [], [], [], [], options);
    
    % Store results
    results{i, 1} = n;
    results{i, 2} = m;
    results{i, 3} = time_ipm;
    results{i, 4} = time_quadprog;
    
    norm_x_qp = norm(x_qp);
    if norm_x_qp > 0
        results{i, 5} = norm(x_ipm - x_qp) / norm_x_qp;
    else
        results{i, 5} = norm(x_ipm - x_qp);
    end
    
    fprintf('Completed. Average time: %.4f s (IPM) and %.4f s (quadprog).\n\n', time_ipm, time_quadprog);
end

% --- 4. Display Final Results ---
disp('=====================================================================================');
disp('                             Final Performance Comparison');
disp('=====================================================================================');
fprintf('%-10s | %-12s | %-20s | %-20s | %-20s\n', ...
    'Variables', 'Constraints', 'IPM Time (s)', 'quadprog Time (s)', 'Relative Sol. Diff.');
fprintf(repmat('-', 1, 95));
fprintf('\n');

for i = 1:length(problem_sizes)
    fprintf('%-10d | %-12d | %-20.6f | %-20.6f | %-20.4e\n', ...
        results{i,1}, results{i,2}, results{i,3}, results{i,4}, results{i,5});
end
disp('=====================================================================================');


%==========================================================================
%                           SOLVER FUNCTIONS
%==========================================================================

%==========================================================================
% 路径跟踪主函数 (Path-Following Method)
%==========================================================================
function [x_opt, r_final] = pathFollowingQP(Q, c, A, b, x0, quiet)
    if nargin < 6
        quiet = false;
    end

    r_val = 1.0;          
    mu = 0.5;               
    R_TOLERANCE = 1e-8; % Looser tolerance for faster convergence on large problems
    MAX_PATH_ITER = 50;     

    x_k = x0; 

    if ~quiet
        fprintf('--- Starting Path-Following Interior-Point Method ---\n');
        fprintf('Path Iter |     r Value     | Centering Norm(g) \n');
        fprintf('-----------------------------------------------------\n');
    end

    for path_k = 1:MAX_PATH_ITER
        % The gradient of f(x) - r * sum(log(s_i)) is grad(f) + r * sum(a_i / s_i)
        % where s_i = b_i - a_i'*x
        Grad_Q_prime = @(x) (Q * x + c) + r_val * (A' * (1 ./ (b - A*x)));
        Hessian_Q_prime = @(x) Q + r_val * (A' * diag(1 ./ (b - A*x).^2) * A);

        [x_center, grad_norm_at_center] = newtonSolver(Grad_Q_prime, Hessian_Q_prime, x_k, A, b);

        if ~quiet
            fprintf('%9d | %15.8e | %15.8e \n', path_k, r_val, grad_norm_at_center);
        end

        if r_val < R_TOLERANCE
            if ~quiet
                fprintf('--- Path-Following Method Converged: r < %e ---\n', R_TOLERANCE);
            end
            x_opt = x_center;
            r_final = r_val;
            return;
        end

        r_val = mu * r_val;
        x_k = x_center; 
    end

    warning('Path-Following Method failed to converge within MAX_PATH_ITER.');
    x_opt = x_center;
    r_final = r_val;
end

%==========================================================================
% 内部牛顿法函数 (newtonSolver)
%==========================================================================
function [x_star, final_grad_norm] = newtonSolver(grad_func, hessian_func, x0, A, b)
    MAX_ITER = 100;
    TOLERANCE = 1e-6; % Looser tolerance for speed
    x_k = x0; 

    alpha = 0.4; % Backtracking factor
    
    final_grad_norm = inf;

    for k = 1:MAX_ITER
        g_k = grad_func(x_k);
        final_grad_norm = norm(g_k);
        
        if final_grad_norm < TOLERANCE
            x_star = x_k;
            return;
        end

        H_k = hessian_func(x_k);
        
        % Add slight regularization for stability, especially in large problems
        H_k = H_k + eye(size(H_k)) * 1e-8; 

        delta_x = -H_k \ g_k;

        t = 1.0; 
        while true
            x_new = x_k + t * delta_x;
            if all((b - A * x_new) > 1e-12) % Check if strictly feasible
                break;
            end
            t = alpha * t; 
            if t < 1e-16
                %warning('Line search failed to find feasible step size.');
                x_star = x_k; % Return last good point
                return;
            end
        end
        x_k = x_new;
    end
    x_star = x_k;
end


%==========================================================================
%                       PROBLEM GENERATION FUNCTION
%==========================================================================
function problem = generateQP(n, m)
    % n: number of variables
    % m: number of inequality constraints (A*x <= b)

    % 1. Create a convex objective function (positive definite Q)
    problem.Q = diag(rand(n, 1) * 9 + 1); % Diagonal entries between 1 and 10
    problem.c = (rand(n, 1) - 0.5) * 20;  % Entries between -10 and 10

    % 2. Create constraints matrix A
    % To ensure the problem is non-trivial, let''s create a sparse matrix
    non_zeros_per_row = min(n, max(3, floor(n / 4))); % Each constraint involves a few variables
    problem.A = zeros(m, n);
    for i = 1:m
        p = randperm(n, non_zeros_per_row);
        problem.A(i, p) = rand(1, non_zeros_per_row) * 2 - 1; % values between -1 and 1
    end

    % 3. Generate a random interior point x0, then build b around it
    % This is a robust way to guarantee a strictly feasible starting point exists.
    problem.x0 = randn(n, 1) * 0.1; % A random point near the origin
    
    % Construct b such that x0 is strictly feasible
    slack = rand(m, 1) * 5 + 1; % random slack between 1 and 6
    problem.b = problem.A * problem.x0 + slack;
end
