use std::str::FromStr;
use std::ffi::CStr;
use std::ffi::CString;
use std::os::raw::c_char;
use std::ptr;

#[derive(Debug)]
enum Operator {
    Add,
    Subtract,
    Multiply,
    Divide,
}

#[derive(Debug)]
struct Expression {
    lhs: f64,
    operator: Operator,
    rhs: f64,
}

impl FromStr for Operator {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "+" => Ok(Operator::Add),
            "-" => Ok(Operator::Subtract),
            "*" => Ok(Operator::Multiply),
            "/" => Ok(Operator::Divide),
            _ => Err(format!("Invalid operator: {}", s)),
        }
    }
}

impl FromStr for Expression {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let parts: Vec<&str> = s.split_whitespace().collect();
        if parts.len() != 3 {
            return Err(format!("Invalid expression format: {}", s));
        }

        let lhs = parts[0].parse::<f64>().map_err(|e| e.to_string())?;
        let operator = parts[1].parse::<Operator>()?;
        let rhs = parts[2].parse::<f64>().map_err(|e| e.to_string())?;

        Ok(Expression { lhs, operator, rhs })
    }
}

impl Expression {
    fn evaluate(&self) -> Result<f64, String> {
        match self.operator {
            Operator::Add => Ok(self.lhs + self.rhs),
            Operator::Subtract => Ok(self.lhs - self.rhs),
            Operator::Multiply => Ok(self.lhs * self.rhs),
            Operator::Divide => {
                if self.rhs == 0.0 {
                    Err("Division by zero".to_string())
                } else {
                    Ok(self.lhs / self.rhs)
                }
            }
        }
    }
}

#[no_mangle]
extern "C" fn calculate_string(expr: *const c_char) -> *const c_char {
    if expr.is_null() {
        return ptr::null();
    }
    let expr_str = unsafe { CStr::from_ptr(expr) };
    let result = match expr_str.to_str() {
        Ok(rust_string) => {
            match rust_string.parse::<Expression>() {
                Ok(expr) => match expr.evaluate() {
                    Ok(result) => format!("{} = {}", rust_string, result),
                    Err(e) => format!("Error evaluating {}: {}", rust_string, e),
                },
                Err(e) => format!("Error parsing {}: {}", rust_string, e),
            }
        }
        Err(e) => {
            format!("Failed to convert CStr to Rust string: {}", e)
        }
    };
    let c_string = match CString::new(result) {
        Ok(s) => s,
        Err(_) => return ptr::null(),
    };
    c_string.into_raw()
}

#[no_mangle]
extern "C" fn calculate_string_free(s: *const c_char) {
    if s.is_null() {
        return;
    }
    unsafe {
        // Recreate the CString from the raw pointer to properly free the memory
        let _ = CString::from_raw(s as *mut c_char);
    }
}
